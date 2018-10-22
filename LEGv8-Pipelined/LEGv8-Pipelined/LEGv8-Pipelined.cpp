//Created By Joshua Slycord
/*
This program was designed to simulate an ARM x64 processor using the limited instruction set LEGv8. The program reads from text files that contain machine code and in future implementation will be able to compile machine language directly from assembly code. These instructions are stored within instruction memory in the program, and once loaded entirely entire the program become the code which the program runs. The program has the option to run the code via pipelined and unpipelined processes, within threading implementation intended as an upcoming feature. Each of these options pass the binary instructions through 4 standard processing stages; fetch, decode, execute, writeback.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
#include <ctime>
#include <array>
#include <thread>
#include <cmath>
#include <iomanip>
#include <map>

using namespace std;

struct Instructions //contains all of the information from the machine code
{
	int opcode;
	int Rd, _Rd; //underscore variables are used to store value stored in corresponding register location
	int Rn, _Rn; // "" 
	int Rm, _Rm; // ""
	int Rt, _Rt; // ""
	int shamt;
	int ALU_immediate;
	int op;
	int BR_address;
	int COND_BR_address;
	int MOV_immediate;
	int DT_address;
	int LSL;
	char format; //Format of the opcode ( R I D B C D )

				 //Overload << operator to be able to output Instructions
				 //Only outputs variables for the proper opcode based on the format
	friend ostream& operator << (ostream& out_str, const Instructions&  output);
};

string fetchVars;
int PC = 1;
int SP = 1;
int clockCycles;
int endProgram = 0;

bool pipeline;
bool negativeFlag;
bool zeroFlag;
bool overflowFlag;
bool carryFlag;
bool branched;
bool concurrentHazardn;
bool concurrentHazardt;
bool concurrentHazardm;
bool storeFlag;
bool loadFlag;
bool atomic;

Instructions decodeVars;
Instructions executeVars;
Instructions writebackVars;
Instructions noOP;
Instructions assemble;

array<int, 32> registers; //Ininitilize 32 registers available to LEG V8
array<string, 100> memory; // Heap/Stack memory used by the assembler, size is arbitrary at this point
array<unsigned char, 1024> dataMemory;
array<int, 200> simpleDataMemory;

int convertBinaryStringToInt(string machineCode, int start, int end, int findFormat, int checkNegative);
string signExtend(string binary, int stringStart, int totalSize);
string convertIntToBinaryString(int integer);
void twosComplement(char *variable);
void finalize(time_t, time_t);
char findFormat(int opcode);
void fillSimpleData();
void readSimpleData();
void storeDataMemory(int data, int location, int size);
int loadDataMemory(int location, int size);
void setFlags(int integer);
void clearFlags();
void checkFlags();
void assembler();
void machineCompliler();
void fetch();
void decode();
void execute();
void writeback();
void clockCycle();
void hazardCheck();
void setup();
void forwarding();
void pipelined();
void unpipelined();



//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////USER DEFINED CODE///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

array<int, 10> unSorted;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////USER DEFINED CODE///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////


int main()
{
	time_t tstart, tend;
	tstart = time(0);

	srand(time(NULL));

	assembler();

	//machineCompliler();

	setup();

	if (pipeline) pipelined();
	else unpipelined();

	tend = time(0);
	finalize(tend, tstart);

	cin;
	return 0;
}

void pipelined()
{
	while (!endProgram)
	{
		fetch();
		decode();
		forwarding();
		execute();
		writeback();

		//clockCycle();
		clockCycles++;
		cout << endl << endl << endl;
		hazardCheck();

	}
}

void unpipelined()
{
	while (!endProgram)
	{
		fetch();
		//clockCycle();
		clockCycles++;
		decode();
		//clockCycle();
		clockCycles++;
		execute();
		//clockCycle();
		clockCycles++;
		writeback();
		//clockCycle();
		clockCycles++;
		cout << endl;
	}
}

void forwarding()
{
	if (concurrentHazardn && writebackVars.format != 'D') executeVars._Rn = writebackVars._Rd;
	if (concurrentHazardm && writebackVars.format != 'D') executeVars._Rm = writebackVars._Rd;
	if (concurrentHazardt && writebackVars.format != 'D')
	{
		if (writebackVars.opcode) executeVars._Rt = writebackVars._Rd;
	}
}

void hazardCheck()
{
	concurrentHazardm = false;
	concurrentHazardn = false;
	concurrentHazardt = false;
	//If statement which checks for back-to-back math functions(I||R) to the same register
	if (
		(PC > 3) && 
		(((decodeVars.Rn == executeVars.Rd) || 
		((decodeVars.Rm == executeVars.Rd) && 
		((decodeVars.format != 'I') && 
		(executeVars.format != 'I')))) && 
		(decodeVars.format == executeVars.format))
		)
	{
		writebackVars = executeVars;
		executeVars = decodeVars;
		executeVars._Rd = registers.at(decodeVars.Rd);
		executeVars._Rm = registers.at(executeVars.Rm);
		executeVars._Rn = registers.at(executeVars.Rn);
		executeVars._Rt = registers.at(executeVars.Rt);
		fetchVars = memory.at(PC);
		if ((decodeVars.Rn == executeVars.Rd))
			concurrentHazardn = true;
		if ((decodeVars.Rm == executeVars.Rd))
			concurrentHazardm = true;
	}
	else if (branched)
	{
		executeVars._Rd = registers.at(decodeVars.Rd);
		executeVars._Rt = registers.at(decodeVars.Rt);
		executeVars._Rm = registers.at(executeVars.Rm);
		executeVars._Rn = registers.at(executeVars.Rn);
		writebackVars = executeVars;
		executeVars = noOP;
		decodeVars = noOP;
		fetchVars = memory.at(PC);
		branched = false;

	}
	else if (((PC > 3) && ((decodeVars.Rt == executeVars.Rd) || (decodeVars.Rm == executeVars.Rd))) && ((decodeVars.format == 'C') && (executeVars.format = 'R')))
	{
		concurrentHazardt = true;
		writebackVars = executeVars;
		executeVars = decodeVars;
		executeVars._Rd = registers.at(decodeVars.Rd);
		executeVars._Rm = registers.at(executeVars.Rm);
		executeVars._Rn = registers.at(executeVars.Rn);
		executeVars._Rt = registers.at(executeVars.Rt);
		fetchVars = memory.at(PC);

	}

	else
	{
		writebackVars = executeVars;
		executeVars = decodeVars;
		executeVars._Rd = registers.at(executeVars.Rd);
		executeVars._Rm = registers.at(executeVars.Rm);
		executeVars._Rn = registers.at(executeVars.Rn);
		executeVars._Rt = registers.at(executeVars.Rt);
		fetchVars = memory.at(PC);
	}
}

void setup()
{
	int tempInput = -1;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////USER DEFINED CODE///////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < 10; i++)
	{
		unSorted[i] = rand() % 100;
		storeDataMemory(unSorted[i], 8 * i, 8);
	}
	cout << endl;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////USER DEFINED CODE///////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	negativeFlag = false;
	zeroFlag = false;
	overflowFlag = false;
	carryFlag = false;
	branched = false;
	storeFlag = false;
	loadFlag = false;
	pipeline = false;
	atomic = false;

	while ((tempInput != 1) && (tempInput != 0))
	{
		cout << "Choose an operating mode: " << endl;
		cout << "0) Unpipelined 1) Pipelined" << endl;
		cin >> tempInput;
		if (tempInput) pipeline = true;
		else pipeline = false;
		cout << endl;
	}

	fetchVars = memory.at(PC);
}

void finalize(time_t tend, time_t tstart)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////USER DEFINED CODE///////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////

	cout << "Before   After" << endl;
	for (int i = 0; i < 10; i++)
	{
		int temp;
		cout << setw(4) << unSorted[i] << "     " << setw(4) << loadDataMemory(i * 8, 8) << endl;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////USER DEFINED CODE///////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	cout << "Clock Cycles: " << clockCycles << " Time Taken: " << difftime(tend, tstart) << " seconds" << endl;

}

void fetch()
{
	if (!pipeline)
	{
		fetchVars = memory.at(PC);
	}

	string border = "==================================Fetch======================================";

	cout << border << endl;
	cout << "PC is at " << PC << endl;
	cout << border << endl;
	PC++;

}

void decode()
{
	if (!pipeline)
	{
		executeVars = decodeVars;
	}

	string border = "==================================Decode=====================================";

	/////////////////////////////////////////////////////
	decodeVars.format = 'X'; //clear op code

	if (decodeVars.format == 'X')
		decodeVars.opcode = convertBinaryStringToInt(fetchVars, 0, 5, 1, 0);
	if (decodeVars.format == 'X')
		decodeVars.opcode = convertBinaryStringToInt(fetchVars, 0, 7, 1, 0);
	if (decodeVars.format == 'X')
		decodeVars.opcode = convertBinaryStringToInt(fetchVars, 0, 8, 1, 0);
	if (decodeVars.format == 'X')
		decodeVars.opcode = convertBinaryStringToInt(fetchVars, 0, 9, 1, 0);
	if (decodeVars.format == 'X')
		decodeVars.opcode = convertBinaryStringToInt(fetchVars, 0, 10, 1, 0);
	/////////////////////////////////////////////////////

	decodeVars.Rd = convertBinaryStringToInt(fetchVars, 27, 31, 0, 0);
	decodeVars.Rn = convertBinaryStringToInt(fetchVars, 22, 26, 0, 0);
	decodeVars.Rm = convertBinaryStringToInt(fetchVars, 11, 15, 0, 0);
	decodeVars.Rt = convertBinaryStringToInt(fetchVars, 27, 31, 0, 0);
	decodeVars.shamt = convertBinaryStringToInt(fetchVars, 16, 21, 0, 0);
	decodeVars.ALU_immediate = convertBinaryStringToInt(fetchVars, 10, 21, 0, 1);
	decodeVars.op = convertBinaryStringToInt(fetchVars, 20, 21, 0, 0);
	decodeVars.BR_address = convertBinaryStringToInt(fetchVars, 6, 31, 0, 1);
	decodeVars.COND_BR_address = convertBinaryStringToInt(fetchVars, 8, 26, 0, 1);
	decodeVars.MOV_immediate = convertBinaryStringToInt(fetchVars, 11, 19, 0, 0);
	decodeVars.DT_address = convertBinaryStringToInt(fetchVars, 11, 19, 0, 1);
	decodeVars.LSL = convertBinaryStringToInt(fetchVars, 9, 10, 0, 0);

	cout << border << endl;
	cout << decodeVars << endl;
	cout << border << endl;

	//cout << decodeVars;
}

void execute()
{
	if (!pipeline)
	{
		executeVars = decodeVars;
		executeVars._Rd = registers.at(executeVars.Rd);
		executeVars._Rm = registers.at(executeVars.Rm);
		executeVars._Rn = registers.at(executeVars.Rn);
		executeVars._Rt = registers.at(executeVars.Rt);
	}

	string border = "==================================Excecute===================================";

	//use opcode to determine operation
	switch (executeVars.opcode)
	{
		//Use _Rn, _Rm, _Rd, _Rt to represent values withing register
		case//B
			0b000101: {/* Format = 'B*/
			PC = PC + executeVars.BR_address; //the -1 accounts for the next fetch increment of PC
			if (pipeline) PC -= 2; //2 additional cycles have passed because of pipeline
			else PC -= 1;
			cout << border << endl;
			cout << "Executing unconditional break to PC: " << executeVars.BR_address << endl;
			cout << border << endl;
			branched = true;
			break; }
		case//FMULS, FDIVS, FCMPS, FADDS, FSUBS, FMULD, FDIVD, FCMPD, FADDD, FSUBD
			0b00011110001: {/* Format = 'R*/
			//This section is for floating point variabls, which are not implemeneted yet in program.
			break; }
		case//BCOND
			0b01010100: {/* Format = 'C*/
			switch (executeVars.Rt)
			{
				case // B.EQ 
					0b00000:
				{
					if (zeroFlag)
					{
						PC = PC + executeVars.COND_BR_address;
						if (pipeline) PC -= 2;
						else PC -= 1;
						clearFlags();
						cout << border << endl;
						cout << "Branching under the condition B.EQ to " << PC << endl;
						cout << border << endl;
						branched = true;
						break;
					}
					else cout << "Branch not taken!" << endl;
					break;
				}
				case // B.NE
					0b00001:
				{
					if (!(zeroFlag))
					{
						PC = PC + executeVars.COND_BR_address;
						if (pipeline) PC -= 2;
						else PC -= 1;
						clearFlags();
						cout << border << endl;
						cout << "Branching under the condition B.NE to " << PC << endl;
						cout << border << endl;
						branched = true;
						break;
					}
					else cout << "Branch not taken!" << endl;
					break;
				}
				case // B.LT
					0b01011:
				{
					if (negativeFlag != overflowFlag)
					{
						PC = PC + executeVars.COND_BR_address;
						if (pipeline) PC -= 2;
						else PC -= 1;
						clearFlags();
						cout << border << endl;
						cout << "Branching under the condition B.LT to " << PC << endl;
						cout << border << endl;
						branched = true;
						break;
					}
					else cout << "Branch not taken!" << endl;
					break;
				}
				case // B.LE
					0b01101:
				{
					if (zeroFlag || (negativeFlag != overflowFlag))
					{
						PC = PC + executeVars.COND_BR_address;
						if (pipeline) PC -= 2;
						else PC -= 1;
						clearFlags();
						cout << border << endl;
						cout << "Branching under the condition B.LE to " << PC << endl;
						cout << border << endl;
						branched = true;
						break;
					}
					else cout << "Branch not taken!" << endl;
					break;
				}
				case // B.GT
					0b01100:
				{
					if ((!zeroFlag) && (negativeFlag == overflowFlag))
					{
						PC = PC + executeVars.COND_BR_address;
						if (pipeline) PC -= 2;
						else PC -= 1;
						clearFlags();
						cout << border << endl;
						cout << "Branching under the condition B.GT to " << PC << endl;
						cout << border << endl;
						branched = true;
						break;
					}
					else cout << "Branch not taken!" << endl;
					break;
				}
				case // B.GE
					0b01010:
				{
					if (negativeFlag == overflowFlag)
					{
						PC = PC + executeVars.COND_BR_address;
						if (pipeline) PC -= 2;
						else PC -= 1;
						clearFlags();
						cout << border << endl;
						cout << "Branching under the condition B.GE to " << PC << endl;
						cout << border << endl;
						branched = true;
						break;
					}
					else cout << "Branch not taken!" << endl;
					break;
				}
				default:
				{
					cout << "No B.Cond found that matches input!" << endl;
					break;
				}
			}
			break;
		}
		case//AND
			0b10001010000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn & executeVars._Rm;
			cout << border << endl;
			cout << "Executing AND between values in register " << executeVars.Rn << " and register " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " & " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//ADD
			0b10001011000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn + executeVars._Rm;
			cout << border << endl;
			cout << "Executing ADD between values in register " << executeVars.Rn << " and register " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " + " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//ADDI
			0b1001000100: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn + executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing ADDI between value in register " << executeVars.Rn << " and immediate value " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " + " << executeVars.ALU_immediate << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//ANDI
			0b1001001000: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn & executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing ANDI between values in register " << executeVars.Rn << " and register " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " & " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//BL
			0b100101: {/* Format = 'B*/
			registers.at(30) = PC + 1;
			PC = PC + executeVars.BR_address;
			cout << border << endl;
			cout << "Storing Link Register from current PC: PC = " << PC + 1 << endl;
			cout << "PC is now " << PC + executeVars.BR_address;
			cout << border << endl;
			branched = true;
			break; }
		case//SDIV, UDIV
			0b10011010110: {/* Format = 'R*/
			switch (executeVars.shamt)
			{
				case//SDIV
					0b000010:
				{
					executeVars._Rd = signed(executeVars._Rn / executeVars._Rm);
					cout << border << endl;
					cout << "Executing Signed Division between values in register " << executeVars.Rn << " and register " << executeVars.ALU_immediate << endl;
					cout << "The results are " << executeVars._Rn << " & " << executeVars._Rm << " = " << executeVars._Rd << endl;
					cout << border << endl;
					break;
				}
				case//UDIV
					0b000011:
				{
					executeVars._Rd = unsigned(executeVars._Rn / executeVars._Rm);
					cout << border << endl;
					cout << "Executing Unsigned Division between values in register " << executeVars.Rn << " and register " << executeVars.ALU_immediate << endl;
					cout << "The results are " << executeVars._Rn << " & " << executeVars._Rm << " = " << executeVars._Rd << endl;
					cout << border << endl;
					break;
				}
				default:
				{
					cout << "Error in shamt for DIV command";
					break;
				}
			}
			break; }
		case//MUL
			0b10011011000: {/* Format = 'R*/
			executeVars._Rd = (executeVars._Rn * executeVars._Rm);
			cout << border << endl;
			cout << "Executing Multiplication on variables in register " << executeVars.Rn << " * " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " * " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//SMULH
			0b10011011010: {/* Format = 'R*/
							//signed
			executeVars._Rd = (signed(executeVars._Rn * executeVars._Rm));
			cout << border << endl;
			cout << "Executing Signed Multiplication High on " << executeVars.Rn << " * " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " * " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//UMULH
			0b10011011110: {/* Format = 'R*/
							//unsigned
			executeVars._Rd = (unsigned(executeVars._Rn * executeVars._Rm));
			cout << border << endl;
			cout << "Executing Unsigned Multiplication High on " << executeVars.Rn << " * " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " * " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//ORR
			0b10101010000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn | executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing Inclusive OR on values in registers " << executeVars.Rn << " OR " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " | " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//ADDS
			0b10101011000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn + executeVars._Rm;
			cout << border << endl;
			cout << "Executing ADD between values in register " << executeVars.Rn << " and register " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " + " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			setFlags(executeVars._Rd);
			break; }
		case//ADDIS
			0b1011000100: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn + executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing ADDI between value in register " << executeVars.Rn << " and immediate value " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " + " << executeVars.ALU_immediate << " = " << executeVars._Rd << endl;
			cout << border << endl;
			setFlags(executeVars._Rd);
			break; }
		case//ORRI
			0b1011001000: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn | executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing Inclusive OR Immediate are " << executeVars.Rn << " OR " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " | " << executeVars.ALU_immediate << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//CBZ
			0b10110100: {/* Format = 'C*/
			if (executeVars._Rt == 0) {
				PC = PC + executeVars.COND_BR_address;
				if (pipeline) PC -= 2;
				else PC -= 1;
				cout << border << endl;
				cout << "Executing Compare & Branch if Zero on value in register " << executeVars.Rt << endl;
				cout << "The new PC is: " << PC << endl;
				cout << border << endl;
				branched = true;
			}
			else
			{
				cout << border << endl;
				cout << "Branch not taken!" << endl;
				cout << border << endl;
			}
			break; }
		case//CBNZ
			0b10110101: {/* Format = 'C*/
			if (executeVars._Rt != 0) {
				PC = PC + executeVars.COND_BR_address;
				if (pipeline) PC -= 2;
				else PC -= 1;
				cout << border << endl;
				cout << "Executing Compare & Branch if Not Zero on value in register  " << executeVars.Rt << endl;
				cout << "The new PC is: " << PC << endl;
				cout << border << endl;
				branched = true;
			}
			else
			{
				cout << border << endl;
				cout << "Branch not taken!" << endl;
				cout << border << endl;
			}
			break; }
		case//STUR
			0b11111000000: {/* Format = 'D*/
			//fillSimpleData();
			if(!atomic) storeDataMemory(executeVars._Rt, (executeVars.DT_address + executeVars._Rn), 8);
			storeFlag = true;
			cout << border << endl;
			cout << "Storing double word at address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The data stored was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//LDUR
			0b11111000010: {/* Format = 'D*/
			//readSimpleData();
			if (!atomic) registers.at(executeVars.Rt) = loadDataMemory((executeVars._Rn + executeVars.DT_address), 8);
			loadFlag = true;
			cout << border << endl;
			cout << "Loading double word from address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The Data loaded was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//STURD
			0b11111100000: {/* Format = 'R*/  break; } //double floating point store
		case//LDURD
			0b11111100010: {/* Format = 'R*/  break; } //double floating point load
		case//STURW
			0b10111000000: {/* Format = 'D*/  
			if (!atomic) storeDataMemory(executeVars._Rt, (executeVars.DT_address + executeVars._Rn), 4);
			storeFlag = true;
			cout << border << endl;
			cout << "Storing word at address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The data stored was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//LDURSW
			0b10111000100: {/* Format = 'D*/ 
			if (!atomic) registers.at(executeVars.Rt) = loadDataMemory((executeVars._Rn + executeVars.DT_address), 4);
			loadFlag = true;
			cout << border << endl;
			cout << "Loading word from address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The Data loaded was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//STURS
			0b10111100000: {/* Format = 'R*/  break; } //single floating point store
		case//LDURS 
			0b10111100010: {/* Format = 'R*/  break; } //single floating point load
		case//STXR
			0b11001000000: {/* Format = 'D*/
			atomic = true;
			if (atomic) storeDataMemory(executeVars._Rt, (executeVars.DT_address + executeVars._Rn), 8);
			storeFlag = true;
			cout << border << endl;
			cout << "Storing atomicaly double word at address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The data stored was " << executeVars._Rt << endl;
			cout << border << endl;
			atomic = false;
			break; }
		case//LDXR
			0b11001000010: {/* Format = 'D*/
			atomic = true;
			if (atomic) registers.at(executeVars.Rt) = loadDataMemory((executeVars._Rn + executeVars.DT_address), 4);
			loadFlag = true;
			cout << border << endl;
			cout << "Loading atomicaly double word from address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The Data loaded was " << executeVars._Rt << endl;
			cout << border << endl;
			atomic = false;
			break; }
		case//STURB
			0b00111000000: {/* Format = 'D*/ 
			if (!atomic) storeDataMemory(executeVars._Rt, (executeVars.DT_address + executeVars._Rn), 1);
			storeFlag = true;
			cout << border << endl;
			cout << "Storing byte at address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The data stored was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//LDURB
			0b00111000010: {/* Format = 'D*/
			if (!atomic) registers.at(executeVars.Rt) = loadDataMemory((executeVars._Rn + executeVars.DT_address), 1);
			loadFlag = true;
			cout << border << endl;
			cout << "Loading byte from address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The Data loaded was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//STURH
			0b01111000000: {/* Format = 'D*/ 
			if (!atomic) storeDataMemory(executeVars._Rt, (executeVars.DT_address + executeVars._Rn), 2);
			storeFlag = true;
			cout << border << endl;
			cout << "Storing half-word at address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The data stored was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//LDURH
			0b01111000010: {/* Format = 'D*/  
			if (!atomic) registers.at(executeVars.Rt) = loadDataMemory((executeVars._Rn + executeVars.DT_address), 2);
			loadFlag = true;
			cout << border << endl;
			cout << "Loading half-word from address in " << executeVars.Rn << " and offsetting by " << executeVars.DT_address << endl;
			cout << "The Data loaded was " << executeVars._Rt << endl;
			cout << border << endl;
			break; }
		case//EOR
			0b11001010000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn ^ executeVars._Rm;
			cout << border << endl;
			cout << "Executing Exclusive OR on values in register " << executeVars.Rn << " OR " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " ^ " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//SUB
			0b11001011000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn - executeVars._Rm;
			cout << border << endl;
			cout << "Executing SUB between values in register " << executeVars.Rn << " and register " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " - " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//SUBI
			0b1101000100: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn - executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing SUBI between value in register " << executeVars.Rn << " and immediate value " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " - " << executeVars.ALU_immediate << " = " << executeVars._Rd << endl;
			cout << border << endl;
			break; }
		case//EORI
			0b1101001000: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn ^ executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing EORI between value in register " << executeVars.Rn << "and immediate value " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " ^ " << executeVars.ALU_immediate << " = " << executeVars._Rd << endl;

			break; }
		case//MOVZ
			0b110100101: {/* Format = 'M*/
		    //R[Rd] = {MOVImm << (Instruction[22:21]*16)}
			executeVars._Rd = executeVars.MOV_immediate << (executeVars.LSL * 16);
			cout << border << endl;
			cout << "Executing MOVZ" << endl;
			cout << border << endl;
			break; }
		case//LSR
			0b11010011010: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn >> executeVars.shamt;
			cout << border << endl;
			cout << "Executing LSR between value in register " << executeVars.Rn << "and shifting it the amount " << executeVars.shamt << endl;
			cout << "The results are " << executeVars._Rn << " Shifted by the amount " << executeVars.shamt << endl;
			break; }
		case//LSL
			0b11010011011: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn << executeVars.shamt;
			cout << "Executing LSL between value in register " << executeVars.Rn << "and shifting it the amount " << executeVars.shamt << endl;
			cout << "The results are " << executeVars._Rn << " Shifted by the amount " << executeVars.shamt << endl;
			break; }
		case//BR
			0b11010110000: {/* Format = 'R*/
			PC = executeVars._Rt;
			cout << border << endl;
			cout << "Executing Branch " << PC << " to register " << executeVars.Rt << endl;
			cout << "The results are " << PC << " = " << executeVars._Rt << endl;
			cout << border << endl;
			branched = true;
			break; }
		case//ANDS
			0b11101010000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn & executeVars._Rm;
			cout << border << endl;
			cout << "Executing ANDS between value in register " << executeVars.Rn << "and register value " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " Compared to the value " << executeVars._Rm << endl;
			cout << border << endl;
			setFlags(executeVars._Rd);
			break; }
		case//SUBS
			0b11101011000: {/* Format = 'R*/
			executeVars._Rd = executeVars._Rn - executeVars._Rm;
			cout << border << endl;
			cout << "Executing SUBS between value in registers " << executeVars.Rn << " and " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " - " << executeVars._Rm << " = " << executeVars._Rd << endl;
			cout << border << endl;
			setFlags(executeVars._Rd);
			break; }
		case//SUBIS
			0b1111000100: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn - executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing SUBIS between value in register " << executeVars.Rn << "and register value " << executeVars.Rm << endl;
			cout << "The results are " << executeVars._Rn << " - " << executeVars._Rm << endl;
			cout << border << endl;
			setFlags(executeVars._Rd);
			break; }
		case//ANDIS
			0b1111001000: {/* Format = 'I*/
			executeVars._Rd = executeVars._Rn & executeVars.ALU_immediate;
			cout << border << endl;
			cout << "Executing ANDIS between value in register " << executeVars.Rn << "and register value " << executeVars.ALU_immediate << endl;
			cout << "The results are " << executeVars._Rn << " compared to " << executeVars.ALU_immediate << endl;
			cout << border << endl;
			setFlags(executeVars._Rd);
			break; }
		case//MOVK
			0b111100101: {/* Format = 'M*/
		    //R[Rd](Instruction[22:21]*16: Instruction[22:21]*16-15) = MOVImmediate
			executeVars._Rd = executeVars._Rd && executeVars.LSL * 16;
			cout << border << endl;
			cout << "Executing MOVZ" << endl;
			cout << border << endl;
			break; }
		case
			0b11111111111: {/* Format = 'X' Custom OPCODE - EXIT PROGRAM */
			endProgram = 1;
			break; }
		default:  cout << border << endl << "No data to be executed" << endl << border << endl;   break;
	}

}

void writeback()
{
	if (!pipeline)
	{
		writebackVars = executeVars;
	}

	string border = "==================================WriteBack==================================";


	/*
	R - Opcode, Rm, shamt, Rn, Rd
	I - Opcode, ALU_immediate, Rn, Rd
	D - Opcode, DT_address, op, Rn, Rt
	B - Opcode, BR_address
	C - Opcode, COND_BR_address, Rt
	W - Opcode, MOV_immediate, Rd
	*/

	if ((writebackVars.format == 'R') || (writebackVars.format == 'I'))
	{
		registers.at(writebackVars.Rd) = writebackVars._Rd;
		cout << border << endl;
		cout << "Writing Values back to register: " << writebackVars.Rd << endl;

	}
	else
	{
		cout << border << endl;
		cout << "No write back to registers needed for instruction" << endl;
	}

	cout << border << endl;
}

string signExtend(string binary, int stringStart, int totalSize)
{//stringStart is == to where string[0] of old string will be located in the new string, counting left to right
	string signExtended(totalSize, binary[0]);
	bool extention = false;
	for (int i = 0; i < totalSize; i++)
	{
		if (i == stringStart)
		{
			for (int j = 0; j < binary.length(); j++)
			{
				signExtended[i + j] = binary[j];
			}
			extention = true;
			i = i + binary.length();
		}
		if (extention)
		{
			signExtended[i] = '0';
		}

	}

	return signExtended;
}

void storeDataMemory(int data, int location, int size)
{//word size is 64 bits
 //array<unsigned char, 8> dataMemory
 //Common Usage
 //storeDataMemory(executeVars._Rt, (executeVars.DT_address + executeVars._Rn), size)


	string temp = convertIntToBinaryString(data);
	array<unsigned char, 8> bytes;

	if (size == 8)
	{
		bytes[0] = convertBinaryStringToInt(temp, 56, 63, 0, 0);
		bytes[1] = convertBinaryStringToInt(temp, 48, 55, 0, 0);
		bytes[2] = convertBinaryStringToInt(temp, 40, 47, 0, 0);
		bytes[3] = convertBinaryStringToInt(temp, 32, 39, 0, 0);
		bytes[4] = convertBinaryStringToInt(temp, 24, 31, 0, 0);
		bytes[5] = convertBinaryStringToInt(temp, 16, 23, 0, 0);
		bytes[6] = convertBinaryStringToInt(temp, 8, 15, 0, 0);
		bytes[7] = convertBinaryStringToInt(temp, 0, 7, 0, 0);
	}

	if (size == 4)
	{
		bytes[0] = convertBinaryStringToInt(temp, 24, 31, 0, 0);
		bytes[1] = convertBinaryStringToInt(temp, 16, 23, 0, 0);
		bytes[2] = convertBinaryStringToInt(temp, 8, 15, 0, 0);
		bytes[3] = convertBinaryStringToInt(temp, 0, 7, 0, 0);
	}

	if (size == 2)
	{
		bytes[0] = convertBinaryStringToInt(temp, 8, 15, 0, 0);
		bytes[1] = convertBinaryStringToInt(temp, 0, 7, 0, 0);
	}

	if (size == 1)
	{
		bytes[0] = convertBinaryStringToInt(temp, 0, 7, 0, 0);
	}

	for (int i = 0; i < size; i++)
	{
		dataMemory[location + i] = bytes[i];
	}
}

int loadDataMemory(int location, int size)
{
	//loadDataMemory(executeVars._Rt, (executeVars._Rn + executeVars.DT_address), size) 
	//load data into executeVars._Rt 

	string combined;
	unsigned char tempChar;
	for (int i = size-1; i >= 0; i--)
	{
		tempChar = dataMemory[location + i];
		combined = combined + (convertIntToBinaryString(tempChar)).substr(56, 8);
	}

	return convertBinaryStringToInt(combined, 0, (size*8)-1, 0, 1);
}

string convertIntToBinaryString(int integer)
{
	string binary = bitset<64>(integer).to_string();
	return binary;
}

void assembler()
{
	//Reads the machine code from file and loads into memory
	ifstream infile;
	infile.open("bubble.machine");
	int memoryLocation = 1;

	string machineCode;

	while (getline(infile, machineCode)) // To get you all the lines.
	{

		if (machineCode[0] == '#') continue;
		else
		{
			remove_if(machineCode.begin(), machineCode.end(), isspace);
			machineCode.resize(32);
			memory.at(memoryLocation) = machineCode;
			memoryLocation++;
		}

	}
	infile.close();
}

void fillSimpleData()
{
	storeFlag = false;
	simpleDataMemory[executeVars._Rn + (executeVars.DT_address / 8)] = executeVars._Rt;
}

void readSimpleData()
{
	loadFlag = false;
	executeVars._Rt = simpleDataMemory[executeVars._Rn + (executeVars.DT_address / 8)];
	registers.at(executeVars.Rt) = executeVars._Rt;
}

int convertBinaryStringToInt(string machineCode, int start, int end, int findFormat, int checkNegative)
{
	bool negative = false;
	char variable[64];
	char *varPtr = &variable[0];
	if (checkNegative)
		fill(variable, variable + 64, machineCode[start]);
	else
		fill(variable, variable + 64, '0');
	char* endptr;
	long int variableInt;
	int length = end - start;

	if (machineCode != "")
	{

		for (int i = 0; i <= length; i++)
		{
			variable[63 - i] = machineCode[end - i];
		}

		if (checkNegative)
		{
			if (variable[0] == '1')
			{
				negative = true;
				for (int i = 0; i < 64; i++)
				{
					if (variable[i] == '0')
					{
						variable[i] = '1';
					}
					else
						variable[i] = '0';
				}
				twosComplement(varPtr);
			}
			

		}

		if (negative)
			variableInt = -1 * strtol(variable, &endptr, 2);
		else
			variableInt = strtol(variable, &endptr, 2);

		negative = false;

		if (findFormat) {
			char format;
			switch (variableInt) {
				case//B
					0b000101: {format = 'B';  break; }
				case//FMULS, FDIVS, FCMPS, FADDS, FSUBS, FMULD, FDIVD, FCMPD, FADDD, FSUBD
					0b00011110001: {format = 'R';  break; }
				case//STURB
					0b00111000000: {format = 'D';  break; }
				case//LDURB
					0b00111000010: {format = 'D';  break; }
				case//BCOND
					0b01010100: {format = 'C';  break; }
				case//STURH
					0b01111000000: {format = 'D';  break; }
				case//LDURH
					0b01111000010: {format = 'D';  break; }
				case//AND
					0b10001010000: {format = 'R';  break; }
				case//ADD
					0b10001011000: {format = 'R';  break; }
				case//ADDI
					0b1001000100: {format = 'I';  break; }
				case//ANDI
					0b1001001000: {format = 'I';  break; }
				case//BL
					0b100101: {format = 'B';  break; }
				case//SDIV, UDIV
					0b10011010110: {format = 'R';  break; }
				case//MUL
					0b10011011000: {format = 'R';  break; }
				case//SMULH
					0b10011011010: {format = 'R';  break; }
				case//UMULH
					0b10011011110: {format = 'R';  break; }
				case//ORR
					0b10101010000: {format = 'R';  break; }
				case//ADDS
					0b10101011000: {format = 'R';  break; }
				case//ADDIS
					0b1011000100: {format = 'I';  break; }
				case//ORRI
					0b1011001000: {format = 'I';  break; }
				case//CBZ
					0b10110100: {format = 'C';  break; }
				case//CBNZ
					0b10110101: {format = 'C';  break; }
				case//STURW
					0b10111000000: {format = 'D';  break; }
				case//LDURSW
					0b10111000100: {format = 'D';  break; }
				case//STURS
					0b10111100000: {format = 'R';  break; }
				case//LDURS
					0b10111100010: {format = 'R';  break; }
				case//STXR
					0b11001000000: {format = 'D';  break; }
				case//LDXR
					0b11001000010: {format = 'D';  break; }
				case//EOR
					0b11001010000: {format = 'R';  break; }
				case//SUB
					0b11001011000: {format = 'R';  break; }
				case//SUBI
					0b1101000100: {format = 'I';  break; }
				case//EORI
					0b1101001000: {format = 'I';  break; }
				case//MOVZ
					0b110100101: {format = 'M';  break; }
				case//LSR
					0b11010011010: {format = 'R';  break; }
				case//LSL
					0b11010011011: {format = 'R';  break; }
				case//BR
					0b11010110000: {format = 'R';  break; }
				case//ANDS
					0b11101010000: {format = 'R';  break; }
				case//SUBS
					0b11101011000: {format = 'R';  break; }
				case//SUBIS
					0b1111000100: {format = 'I';  break; }
				case//ANDIS
					0b1111001000: {format = 'I';  break; }
				case//MOVK
					0b111100101: {format = 'M';  break; }
				case//STUR
					0b11111000000: {format = 'D';  break; }
				case//LDUR
					0b11111000010: {format = 'D';  break; }
				case//STURD
					0b11111100000: {format = 'R';  break; }
				case//LDURD
					0b11111100010: {format = 'R';  break; }
				default: {format = 'X'; break; }
			}
			decodeVars.format = format;
		}

	}
	else
	{
		variableInt = 0;
	}
	return variableInt;
}

void twosComplement(char *variable)
{
	int size = 63;
	bool exit = false;

	while (!exit)
	{
		if (variable[size] == '0')
		{
			variable[size] = '1';
			exit = true;
		}
		else
		{
			variable[size] = '0';
			size--;
		}
	}
}

void clockCycle()
{
	double startTime, endTime, seconds;
	int clockTic = 0;

	startTime = time(NULL);
	while (!clockTic)
	{
		endTime = time(NULL);
		seconds = difftime(endTime, startTime);
		if (seconds >= 1) clockTic = 1;
	}

	return;
}

ostream& operator << (ostream& out_str, const Instructions& output)
{
	if (decodeVars.format == 'R')
	{
		out_str << "Opcode: " << bitset<11>(decodeVars.opcode) << endl
			<< "Rm: " << bitset<5>(decodeVars.Rm) << endl
			<< "shamt: " << bitset<6>(decodeVars.shamt) << endl
			<< "Rn: " << bitset<5>(decodeVars.Rn) << endl
			<< "Rd: " << bitset<5>(decodeVars.Rd) << endl;
	}
	else if (decodeVars.format == 'I')
	{
		out_str << "Opcode: " << bitset<10>(decodeVars.opcode) << endl
			<< "ALU_immediate: " << bitset<12>(decodeVars.ALU_immediate) << endl
			<< "Rn: " << bitset<5>(decodeVars.Rn) << endl
			<< "Rd: " << bitset<5>(decodeVars.Rd) << endl;
	}
	else if (decodeVars.format == 'D')
	{
		out_str << "Opcode: " << bitset<11>(decodeVars.opcode) << endl
			<< "DT_address: " << bitset<9>(decodeVars.Rm) << endl
			<< "op: " << bitset<2>(decodeVars.op) << endl
			<< "Rn: " << bitset<5>(decodeVars.Rn) << endl
			<< "Rt: " << bitset<5>(decodeVars.Rt) << endl;
	}
	else if (decodeVars.format == 'B')
	{
		out_str << "Opcode: " << bitset<6>(decodeVars.opcode) << endl
			<< "BR_address: " << bitset<5>(decodeVars.BR_address) << endl;
	}
	else if (decodeVars.format == 'C')
	{
		out_str << "Opcode: " << bitset<8>(decodeVars.opcode) << endl
			<< "COND_BR_address: " << bitset<19>(decodeVars.COND_BR_address) << endl
			<< "Rt: " << bitset<5>(decodeVars.Rt) << endl;
	}
	else if (decodeVars.format == 'W')
	{
		out_str << "Opcode: " << bitset<11>(decodeVars.opcode) << endl
			<< "MOV_immediate: " << bitset<16>(decodeVars.MOV_immediate) << endl
			<< "Rd: " << bitset<5>(decodeVars.Rd) << endl;
	}
	else
	{
		out_str << "No data to represent" << endl;
	}
	return out_str;
}

void clearFlags()
{
	negativeFlag = false;
	zeroFlag = false;
	overflowFlag = false;
	carryFlag = false;
}

void setFlags(int flagSet)
{
	int f;
	f = flagSet;
	if (flagSet == 0)
	{
		zeroFlag = true;
		cout << "zeroFlag set!" << endl;
	}
	if (flagSet <= 0)
	{
		negativeFlag = true;
		cout << "negativeFlag set!" << endl;
	}
	if ((pow(2, 32) - 1) >= flagSet >= pow(2, 31) - 1)
	{
		carryFlag = true;
		cout << "carryFlag set!" << endl;
	}
	if (flagSet > (pow(2, 32) - 1))
	{
		overflowFlag = true;
		cout << "overflowFlag set!" << endl;
	}

}

void checkFlags()
{
	cout << "The active flags for this instruction cycle are " << endl;
	cout << "Zero flag" << zeroFlag << endl;
	cout << "Negative flag" << negativeFlag << endl;
	cout << "Overflow flag" << overflowFlag << endl;
	cout << "Carry flag" << carryFlag << endl;
}

void machineCompliler()
{
		map<string, int> instruction;

		instruction["B"] = 0b000101;
		instruction["FMULS"] = 0b00011110001;
		instruction["FDIVS"] = 0b00011110001;
		instruction["FCMPS"] = 0b00011110001;
		instruction["FADDS"] = 0b00011110001;
		instruction["FSUBS"] = 0b00011110001;
		instruction["FMULD"] = 0b00011110001;
		instruction["FDIVD"] = 0b00011110001;
		instruction["FDIVD"] = 0b00011110001;
		instruction["FCMPD"] = 0b00011110001;
		instruction["FADDD"] = 0b00011110001;
		instruction["FSUBD"] = 0b00011110001;
		instruction["STURB"] = 0b00111000000;
		instruction["LDURB"] = 0b00111000010;
		instruction["BCOND"] = 0b01010100;
		instruction["STURH"] = 0b01111000000;
		instruction["LDURH"] = 0b01111000010;
		instruction["AND"] = 0b10001010000;
		instruction["ADD"] = 0b10001011000;
		instruction["ADDI"] = 0b1001000100;
		instruction["ANDI"] = 0b1001001000;
		instruction["BL"] = 0b100101;
		instruction["SDIV"] = 0b10011010110;
		instruction["UDIV"] = 0b10011010110;
		instruction["MUL"] = 0b10011011000;
		instruction["SMULH"] = 0b10011011010;
		instruction["UMULH"] = 0b10011011110;
		instruction["ORR"] = 0b10101010000;
		instruction["ADDS"] = 0b10101011000;
		instruction["ADDIS"] = 0b1011000100;
		instruction["ORRI"] = 0b1011001000;
		instruction["CBZ"] = 0b10110100;
		instruction["CBNZ"] = 0b10110101;
		instruction["STURW"] = 0b10111000000;
		instruction["LDURSW"] = 0b10111000100;
		instruction["STURS"] = 0b10111100000;
		instruction["LDURS"] = 0b10111100010;
		instruction["STXR"] = 0b11001000000;
		instruction["LDXR"] = 0b11001000010;
		instruction["EOR"] = 0b11001010000;
		instruction["SUB"] = 0b11001011000;
		instruction["SUBI"] = 0b1101000100;
		instruction["EORI"] = 0b1101001000;
		instruction["MOVZ"] = 0b110100101;
		instruction["LSR"] = 0b11010011010;
		instruction["LSL"] = 0b11010011011;
		instruction["BR"] = 0b11010110000;
		instruction["ANDS"] = 0b11101010000;
		instruction["SUBS"] = 0b11101011000;
		instruction["SUBIS"] = 0b1111000100;
		instruction["ANDIS"] = 0b1111001000;
		instruction["MOVK"] = 0b111100101;
		instruction["STUR"] = 0b11111000000;
		instruction["LDUR"] = 0b11111000010;
		instruction["STURD"] = 0b11111100000;
		instruction["LDURD"] = 0b11111100010;
		instruction["EXIT"] = 0b11111111111;

		instruction["X0"] = 0;
		instruction["X1"] = 1;
		instruction["X2"] = 2;
		instruction["X3"] = 3;
		instruction["X4"] = 4;
		instruction["X5"] = 5;
		instruction["X6"] = 6;
		instruction["X7"] = 7;
		instruction["X8"] = 8;
		instruction["X9"] = 9;
		instruction["X10"] = 10;
		instruction["X11"] = 11;
		instruction["X12"] = 12;
		instruction["X13"] = 13;
		instruction["X14"] = 14;
		instruction["X15"] = 15;
		instruction["X16"] = 16;
		instruction["X17"] = 17;
		instruction["X18"] = 18;
		instruction["X19"] = 19;
		instruction["X20"] = 20;
		instruction["X21"] = 21;
		instruction["X22"] = 22;
		instruction["X23"] = 23;
		instruction["X24"] = 24;
		instruction["X25"] = 25;
		instruction["X26"] = 26;
		instruction["X27"] = 27;
		instruction["X28"] = 28;
		instruction["X29"] = 29;
		instruction["X30"] = 30;
		instruction["X31"] = 31;

		instruction["SP"] = 28;
		instruction["FP"] = 29;
		instruction["LR"] = 30;
		instruction["ZR"] = 31;

		ifstream infile;
		infile.open("bubble.assembly");
		int memoryLocation = 1;

		string assembly;
		string machineCode;

		while (getline(infile, assembly)) // To get you all the lines.
		{
			int i = 0;
			int j = 0;
			string temp;
			char format = 'X';
			bool first, second, third, fourth;

			first = false;
			second = false;
			third = false;
			fourth = false;

			int linelength = 0;

			while (assembly[i] != '\0')
			{
				j = 0;
				linelength = 0;
				while (assembly[j] != '\0')
				{
					linelength++;
					j++;
				}

				if ((assembly[i] == 32) && (!first))
				{
					first = true;
					temp = assembly.substr(0, i);
					auto search = instruction.find(temp);
					if (search != instruction.end())
					{
						format = findFormat(search->second);
						assemble.opcode = search->second;
						assemble.format = format;
					}
					else
						cout << "Format Error!" << endl;
				}
				if ((assembly[i] == 44) && format == 'R')
				{

					auto search = instruction.find(assembly.substr(i - 2, 2));
					if (search != instruction.end())
					{
						if (!second)
						{
							second = true;
							assemble.Rd = search->second;
						}
						else if (second && !third)
						{
							third = true;
							assemble.Rn = search->second;
						}
						else if (third && !fourth)
						{
							fourth = true;
							assemble.Rm = search->second;
						}
					}
					else assemble.shamt = stoi(assembly.substr(i - 2, linelength -i - 2));
				}
				if ((assembly[i] == 44) && format == 'I')
				{
					auto search = instruction.find(assembly.substr(i - 2, 2));
					if (search != instruction.end())
					{
						if (!second)
						{
							second = true;
							assemble.Rd = search->second;
						}
						else if (second && !third)
						{
							third = true;
							assemble.Rn = search->second;
						}
					}
					else
					{
						assemble.ALU_immediate = stoi(assembly.substr(i + 2, linelength - i - 2));
						fourth = true;
					}
				}
				if ((assembly[i] == 44) && format == 'D')
				{
					auto search = instruction.find(assembly.substr(i - 2, 2));
					if (search != instruction.end())
					{
						if (!second)
						{
							second = true;
							assemble.Rt = search->second;
						}
						else if (second && !third)
						{
							third = true;
							assemble.Rn = search->second;
						}
					}
					else
					{
						assemble.DT_address = stoi(assembly.substr(i + 2, linelength - i - 2));
						fourth = true;
					}
				}
				if ((assembly[i] == 44) && format == 'B')
				{
					assemble.BR_address = stoi(assembly.substr(i + 1, 6));
					fourth = true;
				}
				if ((assembly[i] == 44) && format == 'C')
				{
					auto search = instruction.find(assembly.substr(i - 2, 2));
					if (search != instruction.end())
					{
						if (!second)
						{
							second = true;
							assemble.Rt = search->second;
						}
					}
					else
					{
						assemble.COND_BR_address = stoi(assembly.substr(i + 2, linelength - i - 2));
						fourth = true;
					}
				}
				if ((assembly[i] == 44) && format == 'M')
				{
					auto search = instruction.find(assembly.substr(i - 2, 2));
					if (search != instruction.end())
					{
						if (!second)
						{
							second = true;
							assemble.Rd = search->second;
						}
					}
					else
					{
						assemble.MOV_immediate = stoi(assembly.substr(i + 2, linelength - i - 2));
						fourth = true;
					}
				}
				i++;
			}
			

			///////

			if (fourth)
			{
				if (format == 'R')
				{
					machineCode = bitset<11>(assemble.opcode).to_string();
					machineCode += bitset<5>(assemble.Rm).to_string();
					machineCode += bitset<6>(assemble.shamt).to_string();
					machineCode += bitset<5>(assemble.Rn).to_string();
					machineCode += bitset<5>(assemble.Rd).to_string();
				}
				if (format == 'I')
				{
					machineCode = bitset<10>(assemble.opcode).to_string();
					machineCode += bitset<12>(assemble.ALU_immediate).to_string();
					machineCode += bitset<5>(assemble.Rn).to_string();
					machineCode += bitset<5>(assemble.Rd).to_string();
				}
				if (format == 'D')
				{
					machineCode = bitset<11>(assemble.opcode).to_string();
					machineCode += bitset<9>(assemble.DT_address).to_string();
					machineCode += "00";
					machineCode += bitset<5>(assemble.Rn).to_string();
					machineCode += bitset<5>(assemble.Rt).to_string();
				}
				if (format == 'B')
				{
					machineCode = bitset<6>(assemble.opcode).to_string();
					machineCode += bitset<26>(assemble.BR_address).to_string();
				}
				if (format == 'C')
				{
					machineCode = bitset<8>(assemble.opcode).to_string();
					machineCode += bitset<19>(assemble.COND_BR_address).to_string();
					machineCode += bitset<5>(assemble.Rd).to_string();
				}
				if (format == 'M')
				{
					machineCode = bitset<11>(assemble.opcode).to_string();
					machineCode += bitset<16>(assemble.MOV_immediate).to_string();
					machineCode += bitset<5>(assemble.Rd).to_string();
				}


				memory.at(memoryLocation) = machineCode;
				memoryLocation++;
			}
		}

		infile.close();

	}

char findFormat(int opcode)
	{

		char format;
		switch (opcode) {
			case//B
				0b000101: {format = 'B';  break; }
			case//FMULS, FDIVS, FCMPS, FADDS, FSUBS, FMULD, FDIVD, FCMPD, FADDD, FSUBD
				0b00011110001: {format = 'R';  break; }
			case//STURB
				0b00111000000: {format = 'D';  break; }
			case//LDURB
				0b00111000010: {format = 'D';  break; }
			case//BCOND
				0b01010100: {format = 'C';  break; }
			case//STURH
				0b01111000000: {format = 'D';  break; }
			case//LDURH
				0b01111000010: {format = 'D';  break; }
			case//AND
				0b10001010000: {format = 'R';  break; }
			case//ADD
				0b10001011000: {format = 'R';  break; }
			case//ADDI
				0b1001000100: {format = 'I';  break; }
			case//ANDI
				0b1001001000: {format = 'I';  break; }
			case//BL
				0b100101: {format = 'B';  break; }
			case//SDIV, UDIV
				0b10011010110: {format = 'R';  break; }
			case//MUL
				0b10011011000: {format = 'R';  break; }
			case//SMULH
				0b10011011010: {format = 'R';  break; }
			case//UMULH
				0b10011011110: {format = 'R';  break; }
			case//ORR
				0b10101010000: {format = 'R';  break; }
			case//ADDS
				0b10101011000: {format = 'R';  break; }
			case//ADDIS
				0b1011000100: {format = 'I';  break; }
			case//ORRI
				0b1011001000: {format = 'I';  break; }
			case//CBZ
				0b10110100: {format = 'C';  break; }
			case//CBNZ
				0b10110101: {format = 'C';  break; }
			case//STURW
				0b10111000000: {format = 'D';  break; }
			case//LDURSW
				0b10111000100: {format = 'D';  break; }
			case//STURS
				0b10111100000: {format = 'R';  break; }
			case//LDURS
				0b10111100010: {format = 'R';  break; }
			case//STXR
				0b11001000000: {format = 'D';  break; }
			case//LDXR
				0b11001000010: {format = 'D';  break; }
			case//EOR
				0b11001010000: {format = 'R';  break; }
			case//SUB
				0b11001011000: {format = 'R';  break; }
			case//SUBI
				0b1101000100: {format = 'I';  break; }
			case//EORI
				0b1101001000: {format = 'I';  break; }
			case//MOVZ
				0b110100101: {format = 'M';  break; }
			case//LSR
				0b11010011010: {format = 'R';  break; }
			case//LSL
				0b11010011011: {format = 'R';  break; }
			case//BR
				0b11010110000: {format = 'R';  break; }
			case//ANDS
				0b11101010000: {format = 'R';  break; }
			case//SUBS
				0b11101011000: {format = 'R';  break; }
			case//SUBIS
				0b1111000100: {format = 'I';  break; }
			case//ANDIS
				0b1111001000: {format = 'I';  break; }
			case//MOVK
				0b111100101: {format = 'M';  break; }
			case//STUR
				0b11111000000: {format = 'D';  break; }
			case//LDUR
				0b11111000010: {format = 'D';  break; }
			case//STURD
				0b11111100000: {format = 'R';  break; }
			case//LDURD
				0b11111100010: {format = 'R';  break; }
			default: {format = 'X'; break; }
		}

		return format;
	}
