package main

import "fmt"

type IFileSystemNode interface {
	GetSize() int
	Rename(name string)
	PrintDetails()
}

type File struct {
	Name    string
	Size    int
	Content string
}

func NewFile(name string, size int) *File {
	return &File{Name: name, Size: size}
}

func (f *File) GetSize() int {
	return f.Size
}

func (f *File) Rename(name string) {
	f.Name = name
}

func (f *File) PrintDetails() {
	fmt.Println("File:", f.Name)
}

type Folder struct {
	Name   string
	Childs map[string]IFileSystemNode
}

func NewFolder(name string) *Folder {
	return &Folder{Name: name, Childs: make(map[string]IFileSystemNode)}
}

func (fo *Folder) Mkdir(name string) {
	fo.Childs[name] = NewFolder(name)
}

func (fo *Folder) Touch(name string, size int) {
	fo.Childs[name] = NewFile(name, size)
}

func (fo *Folder) DeleteFileSystemNode(name string) {
	delete(fo.Childs, name)
}

func (fo *Folder) GetChild(name string) IFileSystemNode {
	return fo.Childs[name]
}

func (fo *Folder) GetSize() int {
	size := 0
	for _, node := range fo.Childs {
		size += node.GetSize()
	}
	return size
}

func (fo *Folder) Rename(name string) {
	fmt.Println("renamed")
}

func (fo *Folder) PrintDetails() {
	fmt.Println("Current Folder:", fo.Name)
	for _, node := range fo.Childs {
		node.PrintDetails()
	}
}

func main() {
	root := NewFolder("root")
	root.Mkdir("movies")
}
