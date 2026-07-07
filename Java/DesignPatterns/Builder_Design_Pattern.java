package Java.DesignPatterns;


//Product 

class Student{
    private String name; // Mandatory 
    private int rollNumber; //Optional Parameter
    private int age; //Optiona Parameter
    private String address; //Optional Parameter
    private Student(StudentBuilder builder){
        this.name = builder.name;
        this.rollNumber = builder.rollNumber;
        this.age = builder.age;
        this.address = builder.address;
    }

    static class StudentBuilder{
        private String name; // Mandatory 
        private int rollNumber; //Optional Parameter
        private int age; //Optiona Parameter
        private String address; //Optional Parameter

        public StudentBuilder(String name){
         this.name = name; 
        }

        StudentBuilder setRollNumber(int rollNumber){
            this.rollNumber =  rollNumber;
            return this;
        }
        StudentBuilder setAge(int age){
            this.age =  age;
            return this;
        }
        StudentBuilder setAddress(String address){
            this.address =  address;
            return this;
        }

        Student build(){
            return new Student(this);
        }


    }

}
public class Builder_Design_Pattern {
    public static void main(String[] args) {
        Student student = new Student.StudentBuilder("pradeep")
                                     .setAddress("dada")
                                     .setAge(123)
                                     .setAddress("dads")
                                     .build();

        //Is modification of stuudent possible..
        
    }
}
