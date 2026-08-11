/*
================================================================================
LLD: In-Memory File System
================================================================================

Functional Requirements:
    1. Create a directory at a given path.
    2. Create a file at a given path.
    3. Write (overwrite) content to a file.
    4. Read the content of a file.
    5. Delete a file or a directory (recursively).
    6. Calculate the size of a file or a directory (recursive sum).
    7. Move a file or directory from one path to another.
    8. Enforce permissions at the directory level for ADMIN vs USER.

Non-Functional Requirements:
    1. Extensibility: adding a new node type (e.g. symlink) shouldn't
       require touching path-resolution logic.
    2. Predictable errors: an operation on a path that doesn't exist, or
       one a user lacks permission for, fails with a clear exception
       instead of corrupting the tree.

Design:
    FileSystemNode is the common interface File and Directory both
    satisfy -- both know their own size. Directory additionally holds a
    name -> FileSystemNode map of children and a RequiredRole: an
    operation that mutates something under this directory (or the
    directory itself) must be performed by a user whose role is at least
    RequiredRole.

    FileSystemManager resolves "/"-separated paths by walking the tree
    from the root and performs permission checks against the *parent*
    directory of the node being mutated (create/write/delete/move all
    change the parent's contents).

Core Entities:
    UserType / User
    FileSystemNode (interface) / File / Directory
    FileSystemManager
================================================================================
*/

package main

import (
	"errors"
	"fmt"
	"strings"
)

type UserType int

const (
	RoleUser UserType = iota
	RoleAdmin
)

func (t UserType) String() string {
	if t == RoleAdmin {
		return "ADMIN"
	}
	return "USER"
}

type User struct {
	Name string
	Role UserType
}

type FileSystemNode interface {
	Name() string
	SetName(name string)
	Parent() *Directory
	SetParent(parent *Directory)
	GetSize() int
	IsDirectory() bool
}

type baseNode struct {
	name   string
	parent *Directory
}

func (n *baseNode) Name() string           { return n.name }
func (n *baseNode) SetName(name string)    { n.name = name }
func (n *baseNode) Parent() *Directory     { return n.parent }
func (n *baseNode) SetParent(p *Directory) { n.parent = p }

type File struct {
	baseNode
	Content string
}

func NewFile(name string, parent *Directory) *File {
	return &File{baseNode: baseNode{name: name, parent: parent}}
}

func (f *File) Read() string          { return f.Content }
func (f *File) Write(content string)  { f.Content = content }
func (f *File) Append(content string) { f.Content += content }
func (f *File) GetSize() int          { return len(f.Content) }
func (f *File) IsDirectory() bool     { return false }

type Directory struct {
	baseNode
	Children     map[string]FileSystemNode
	RequiredRole UserType
}

func NewDirectory(name string, parent *Directory, requiredRole UserType) *Directory {
	return &Directory{baseNode: baseNode{name: name, parent: parent},
		Children: make(map[string]FileSystemNode), RequiredRole: requiredRole}
}

func (d *Directory) AddChild(child FileSystemNode)       { d.Children[child.Name()] = child }
func (d *Directory) GetChild(name string) FileSystemNode { return d.Children[name] }
func (d *Directory) RemoveChild(name string)             { delete(d.Children, name) }

func (d *Directory) GetSize() int {
	total := 0
	for _, child := range d.Children {
		total += child.GetSize()
	}
	return total
}

func (d *Directory) IsDirectory() bool { return true }

var (
	ErrPathNotFound      = errors.New("path not found")
	ErrPathAlreadyExists = errors.New("path already exists")
	ErrPermissionDenied  = errors.New("permission denied")
)

func splitPath(path string) ([]string, error) {
	raw := strings.Split(path, "/")
	var parts []string
	for _, part := range raw {
		if part != "" {
			parts = append(parts, part)
		}
	}
	if len(parts) == 0 {
		return nil, errors.New("path must not be empty")
	}
	return parts, nil
}

type FileSystemManager struct {
	Root *Directory
}

func NewFileSystemManager() *FileSystemManager {
	return &FileSystemManager{Root: NewDirectory("/", nil, RoleUser)}
}

func (m *FileSystemManager) resolveParent(parts []string) (*Directory, error) {
	current := m.Root
	for _, part := range parts[:len(parts)-1] {
		child := current.GetChild(part)
		dir, ok := child.(*Directory)
		if child == nil || !ok {
			return nil, fmt.Errorf("%w: %s", ErrPathNotFound, part)
		}
		current = dir
	}
	return current, nil
}

func (m *FileSystemManager) resolve(path string) (FileSystemNode, error) {
	parts, err := splitPath(path)
	if err != nil {
		return nil, err
	}
	var current FileSystemNode = m.Root
	for _, part := range parts {
		dir, ok := current.(*Directory)
		if !ok {
			return nil, fmt.Errorf("%w: not a directory: %s", ErrPathNotFound, current.Name())
		}
		child := dir.GetChild(part)
		if child == nil {
			return nil, fmt.Errorf("%w: %s", ErrPathNotFound, path)
		}
		current = child
	}
	return current, nil
}

func (m *FileSystemManager) checkPermission(directory *Directory, user User) error {
	if user.Role < directory.RequiredRole {
		return fmt.Errorf("%w: user %s (%s) lacks permission for %s (requires %s)",
			ErrPermissionDenied, user.Name, user.Role, directory.Name(), directory.RequiredRole)
	}
	return nil
}

func (m *FileSystemManager) CreateDirectory(user User, path string, requiredRole UserType) (*Directory, error) {
	parts, err := splitPath(path)
	if err != nil {
		return nil, err
	}
	parent, err := m.resolveParent(parts)
	if err != nil {
		return nil, err
	}
	if err := m.checkPermission(parent, user); err != nil {
		return nil, err
	}
	name := parts[len(parts)-1]
	if parent.GetChild(name) != nil {
		return nil, fmt.Errorf("%w: %s", ErrPathAlreadyExists, path)
	}
	directory := NewDirectory(name, parent, requiredRole)
	parent.AddChild(directory)
	return directory, nil
}

func (m *FileSystemManager) CreateFile(user User, path string) (*File, error) {
	parts, err := splitPath(path)
	if err != nil {
		return nil, err
	}
	parent, err := m.resolveParent(parts)
	if err != nil {
		return nil, err
	}
	if err := m.checkPermission(parent, user); err != nil {
		return nil, err
	}
	name := parts[len(parts)-1]
	if parent.GetChild(name) != nil {
		return nil, fmt.Errorf("%w: %s", ErrPathAlreadyExists, path)
	}
	file := NewFile(name, parent)
	parent.AddChild(file)
	return file, nil
}

func (m *FileSystemManager) WriteFile(user User, path, content string) error {
	node, err := m.resolve(path)
	if err != nil {
		return err
	}
	file, ok := node.(*File)
	if !ok {
		return fmt.Errorf("not a file: %s", path)
	}
	if err := m.checkPermission(file.Parent(), user); err != nil {
		return err
	}
	file.Write(content)
	return nil
}

func (m *FileSystemManager) ReadFile(path string) (string, error) {
	node, err := m.resolve(path)
	if err != nil {
		return "", err
	}
	file, ok := node.(*File)
	if !ok {
		return "", fmt.Errorf("not a file: %s", path)
	}
	return file.Read(), nil
}

func (m *FileSystemManager) Delete(user User, path string) error {
	node, err := m.resolve(path)
	if err != nil {
		return err
	}
	parent := node.Parent()
	if parent == nil {
		return errors.New("cannot delete the root directory")
	}
	if err := m.checkPermission(parent, user); err != nil {
		return err
	}
	parent.RemoveChild(node.Name())
	return nil
}

func (m *FileSystemManager) GetSize(path string) (int, error) {
	node, err := m.resolve(path)
	if err != nil {
		return 0, err
	}
	return node.GetSize(), nil
}

func (m *FileSystemManager) Move(user User, source, destination string) error {
	node, err := m.resolve(source)
	if err != nil {
		return err
	}
	oldParent := node.Parent()
	if oldParent == nil {
		return errors.New("cannot move the root directory")
	}
	if err := m.checkPermission(oldParent, user); err != nil {
		return err
	}

	destParts, err := splitPath(destination)
	if err != nil {
		return err
	}
	newParent, err := m.resolveParent(destParts)
	if err != nil {
		return err
	}
	if err := m.checkPermission(newParent, user); err != nil {
		return err
	}
	newName := destParts[len(destParts)-1]
	if newParent.GetChild(newName) != nil {
		return fmt.Errorf("%w: %s", ErrPathAlreadyExists, destination)
	}

	oldParent.RemoveChild(node.Name())
	node.SetName(newName)
	node.SetParent(newParent)
	newParent.AddChild(node)
	return nil
}

func main() {
	fs := NewFileSystemManager()
	admin := User{Name: "root-admin", Role: RoleAdmin}
	guest := User{Name: "guest", Role: RoleUser}

	fs.CreateDirectory(admin, "/docs", RoleUser)
	fs.CreateFile(admin, "/docs/notes.txt")
	fs.WriteFile(admin, "/docs/notes.txt", "Meeting notes: LLD review at 3pm")

	fs.CreateDirectory(admin, "/docs/drafts", RoleUser)
	fs.CreateFile(admin, "/docs/drafts/todo.txt")
	fs.WriteFile(admin, "/docs/drafts/todo.txt", "1. Finish parser\n2. Write tests")

	size, _ := fs.GetSize("/docs")
	fmt.Printf("/docs size: %d bytes\n", size)

	fmt.Println("\nCreating an admin-only directory and writing to it as a guest:")
	fs.CreateDirectory(admin, "/secure", RoleAdmin)
	fs.CreateFile(admin, "/secure/keys.txt")
	if err := fs.WriteFile(guest, "/secure/keys.txt", "should not be allowed"); err != nil {
		fmt.Printf("  %s\n", err)
	}

	fmt.Println("\nWriting to it as an admin succeeds:")
	fs.WriteFile(admin, "/secure/keys.txt", "api-key-12345")
	content, _ := fs.ReadFile("/secure/keys.txt")
	fmt.Printf("  /secure/keys.txt -> '%s'\n", content)

	fmt.Println("\nMoving /docs/drafts/todo.txt to /docs/todo.txt:")
	fs.Move(admin, "/docs/drafts/todo.txt", "/docs/todo.txt")
	content, _ = fs.ReadFile("/docs/todo.txt")
	fmt.Printf("  /docs/todo.txt -> '%s'\n", content)

	fmt.Println("\nDeleting /docs/drafts (now empty):")
	fs.Delete(admin, "/docs/drafts")
	size, _ = fs.GetSize("/docs")
	fmt.Printf("  /docs size after cleanup: %d bytes\n", size)
}
