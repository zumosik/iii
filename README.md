


# iii
This repository contains an interpreter for "iii" programming language 

<!-- TOC start -->
- [iii](#iii)
- [About language ](#about-language)
- [1. Installation ](#1-installation)
   * [Usage](#usage)
- [2. Syntax](#2-syntax)
   * [2.1 Definitions ](#21-definitions)
   * [2.2 Arithmetic operations ](#22-arithmetic-operations)
   * [2.3 Builtin functions](#23-builtin-functions)
   * [2.5 Functions ](#25-functions)
   * [2.6 If-else statements ](#26-if-else-statements)
   * [2.7 Loops](#27-loops)
      + [For ](#for)
      + [While ](#while)
   * [2.9 Comments](#29-comments)
   * [Class system ](#class-system)
      + [Basics](#basics)
      + [Inheritance](#inheritance)
<!-- TOC end -->
# About language 
**iii** language is:
- high-level
- open source
- object-oriented
- dynamically typed 
- garbage-collected
# 1. Installation 
You will need **C compiler** and **Make**
To compile interpreter:
```sh
make MODE=release
```
 If you want to compile it in debug mode 
```sh
make MODE=debug
``` 

In both variants it will produce binary "./build/iii"

## Usage
If passed 0 arguments will open REPL
```sh
./iii
```
If argument is passed it will be treated as filename of file for interpreter to run
```sh
./iii <filename>
``` 

# 2. Syntax
**NOTE**: Example-programs can be found beneath [examples/](examples/) which demonstrate these things.
## 2.1 Definitions 
Variables are defines using the 'var' keyword. With each line ending with ';'.
```
var a = 5.5;
var b = true;
var c = "Hello world!";
```
Variables may be floats, booleans or strings.
Global variables may be updated using with `var` keyword.
Local variables may be updated only without `var` keyword.
``
var x = 4;
var x = 3; // would work if variable is global
x = 5;     // would work with any variable
``
## 2.2 Arithmetic operations 
**iii** supports all the basic arithmetic operation of 'float' types.
```
var x = 4.4;
var y = 7.3;

print(x + y); // 11.7
print(y - x); // 2.9
print(x / y); // 0.60274
print(x * y); // 32.12
``` 
## 2.3 Builtin functions
All builtin functions:
* `print`
	* Prints values to stdout 
* `clock`
	* Returns elapsed processor time in seconds since the program started

## 2.5 Functions 
**iii** uses `fn` followed by a name to define a function.
```
fn add(a,b) { return a + b; }
print(add(4,5)); // 7
```

## 2.6 If-else statements 
**iii** supports if-else statements 
```
var a = 4;
var b = 5;

if (a > b) {
	print("a > b");
} else {
	print("a < b");
} 
```

## 2.7 Loops
There is 2 types of loop `for` and `while`
### For 
```
for (<initializer>; <condition>; <increment>) { <body> }
```
initializer, condition and increment are optional.
```
for (var x = 0; x < 100; x = x + 1) { print(x); } // basic for loop 
for (;;) { print("Hello from infinte loop"); }    // infinite for loop
``` 
### While 
```
var x = 3;
while(x != 7) { // basic while loop
	print("X now is: ", x);
	x = x + 1;
}

while(true) { // infinite while loop
	print("infinite...");
}
```
## 2.9 Comments
To create single line comment use `//`.
**iii** don't have multi line comments.

## Class system 
**iii** is object-oriented and have class system
### Basics
```
class Person {};  // defining class
var x = Person(); // creating instance of Person
```
Class can have init method 
```
class Person {
	init(firstname, lastname) { // defining init method
		this.firstname = firstname; // to set property use this followed by '.' and property name 
		this.lastname = lastname;
		this.fullname = this.firstname + " " + this.lastname;
	}
}

var x = Person("James", "Raven"); // all arguments will be passed to defined init function
print(x.fullname); // you can access properies
```
And you can define custom methods
```
class Person {
// ...
	printName() { // defining custom method
		print(this.fullname);
	}
}
// ...
x.printName(); // calling method
var c = x.printName; // or you can save method as var 
c(); // and call it using var
```
### Inheritance
**iii** supports class inheritance 
```
class Student - Person { // use '-' for inheritance 
	// now Student is child class
	// and Person is parent class
}

var y = Student(); // create instance of Student
y.printName(); // call method defined in parent class 
```
```
class Student - Person {
	init(grade, name, lastname) {
		this.grade = grade;
		super.init(name, lastname); // we can call methods of parent using 'super' keyword
	}

	printGrade() {
		print("Class of ", this.fullname, " is ", this.grade); // we can access properties of parent 
		// we could do same using 'super.fullname'
	}
}

var y = Student(3, "Michael", "Raven");
y.printGrade(); // method of Student
y.printName();  // method of Person
```

