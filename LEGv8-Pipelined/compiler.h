#ifndef COMPILER_H
#define COMPILER_H



int assemblyFill(std::string assembly, int i, int j);
char findFormat(int opcode);

int customDelimiter = 47; //set to / for now, will be customizable
Instructions compliledAssembly;



#endif // COMPILER_H
