//===-- SparcAsmPrinter.cpp - Sparc LLVM assembly writer ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format SPARC assembly language.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "asm-printer"
#include "Sparc.h"
#include "SparcInstrInfo.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Target/TargetAsmInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/Mangler.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"
#include <cctype>
#include <cstring>
using namespace llvm;

STATISTIC(EmittedInsts, "Number of machine instrs printed");

namespace {
  struct VISIBILITY_HIDDEN SparcAsmPrinter : public AsmPrinter {
    SparcAsmPrinter(std::ostream &O, TargetMachine &TM, const TargetAsmInfo *T)
      : AsmPrinter(O, TM, T) {
    }

    /// We name each basic block in a Function with a unique number, so
    /// that we can consistently refer to them later. This is cleared
    /// at the beginning of each call to runOnMachineFunction().
    ///
    typedef std::map<const Value *, unsigned> ValueMapTy;
    ValueMapTy NumberForBB;

    virtual const char *getPassName() const {
      return "Sparc Assembly Printer";
    }

    void printOperand(const MachineInstr *MI, int opNum);
    void printMemOperand(const MachineInstr *MI, int opNum,
                         const char *Modifier = 0);
    void printCCOperand(const MachineInstr *MI, int opNum);

    bool printInstruction(const MachineInstr *MI);  // autogenerated.
    bool runOnMachineFunction(MachineFunction &F);
    bool doInitialization(Module &M);
    bool doFinalization(Module &M);
  };
} // end of anonymous namespace

#include "SparcGenAsmWriter.inc"

/// createSparcCodePrinterPass - Returns a pass that prints the SPARC
/// assembly code for a MachineFunction to the given output stream,
/// using the given target machine description.  This should work
/// regardless of whether the function is in SSA form.
///
FunctionPass *llvm::createSparcCodePrinterPass(std::ostream &o,
                                               TargetMachine &tm) {
  return new SparcAsmPrinter(o, tm, tm.getTargetAsmInfo());
}

/// runOnMachineFunction - This uses the printInstruction()
/// method to print assembly for each instruction.
///
bool SparcAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  SetupMachineFunction(MF);

  // Print out constants referenced by the function
  EmitConstantPool(MF.getConstantPool());

  // BBNumber is used here so that a given Printer will never give two
  // BBs the same name. (If you have a better way, please let me know!)
  static unsigned BBNumber = 0;

  O << "\n\n";
  // What's my mangled name?
  CurrentFnName = Mang->getValueName(MF.getFunction());

  // Print out the label for the function.
  const Function *F = MF.getFunction();
  SwitchToTextSection(getSectionForFunction(*F).c_str(), F);
  EmitAlignment(4, F);
  O << "\t.globl\t" << CurrentFnName << "\n";
  O << "\t.type\t" << CurrentFnName << ", #function\n";
  O << CurrentFnName << ":\n";

  // Number each basic block so that we can consistently refer to them
  // in PC-relative references.
  // FIXME: Why not use the MBB numbers?
  NumberForBB.clear();
  for (MachineFunction::const_iterator I = MF.begin(), E = MF.end();
       I != E; ++I) {
    NumberForBB[I->getBasicBlock()] = BBNumber++;
  }

  // Print out code for the function.
  for (MachineFunction::const_iterator I = MF.begin(), E = MF.end();
       I != E; ++I) {
    // Print a label for the basic block.
    if (I != MF.begin()) {
      printBasicBlockLabel(I, true, true);
      O << '\n';
    }
    for (MachineBasicBlock::const_iterator II = I->begin(), E = I->end();
         II != E; ++II) {
      // Print the assembly for the instruction.
      printInstruction(II);
      ++EmittedInsts;
    }
  }

  // We didn't modify anything.
  return false;
}

void SparcAsmPrinter::printOperand(const MachineInstr *MI, int opNum) {
  const MachineOperand &MO = MI->getOperand (opNum);
  const TargetRegisterInfo &RI = *TM.getRegisterInfo();
  bool CloseParen = false;
  if (MI->getOpcode() == SP::SETHIi && !MO.isRegister() && !MO.isImmediate()) {
    O << "%hi(";
    CloseParen = true;
  } else if ((MI->getOpcode() == SP::ORri || MI->getOpcode() == SP::ADDri)
             && !MO.isRegister() && !MO.isImmediate()) {
    O << "%lo(";
    CloseParen = true;
  }
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (TargetRegisterInfo::isPhysicalRegister(MO.getReg()))
      O << "%" << LowercaseString (RI.get(MO.getReg()).AsmName);
    else
      O << "%reg" << MO.getReg();
    break;

  case MachineOperand::MO_Immediate:
    O << (int)MO.getImm();
    break;
  case MachineOperand::MO_MachineBasicBlock:
    printBasicBlockLabel(MO.getMBB());
    return;
  case MachineOperand::MO_GlobalAddress:
    O << Mang->getValueName(MO.getGlobal());
    break;
  case MachineOperand::MO_ExternalSymbol:
    O << MO.getSymbolName();
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    O << TAI->getPrivateGlobalPrefix() << "CPI" << getFunctionNumber() << "_"
      << MO.getIndex();
    break;
  default:
    O << "<unknown operand type>"; abort (); break;
  }
  if (CloseParen) O << ")";
}

void SparcAsmPrinter::printMemOperand(const MachineInstr *MI, int opNum,
                                      const char *Modifier) {
  printOperand(MI, opNum);
  
  // If this is an ADD operand, emit it like normal operands.
  if (Modifier && !strcmp(Modifier, "arith")) {
    O << ", ";
    printOperand(MI, opNum+1);
    return;
  }
  
  if (MI->getOperand(opNum+1).isRegister() &&
      MI->getOperand(opNum+1).getReg() == SP::G0)
    return;   // don't print "+%g0"
  if (MI->getOperand(opNum+1).isImmediate() &&
      MI->getOperand(opNum+1).getImm() == 0)
    return;   // don't print "+0"
  
  O << "+";
  if (MI->getOperand(opNum+1).isGlobalAddress() ||
      MI->getOperand(opNum+1).isConstantPoolIndex()) {
    O << "%lo(";
    printOperand(MI, opNum+1);
    O << ")";
  } else {
    printOperand(MI, opNum+1);
  }
}

void SparcAsmPrinter::printCCOperand(const MachineInstr *MI, int opNum) {
  int CC = (int)MI->getOperand(opNum).getImm();
  O << SPARCCondCodeToString((SPCC::CondCodes)CC);
}



bool SparcAsmPrinter::doInitialization(Module &M) {
  Mang = new Mangler(M);
  return false; // success
}

bool SparcAsmPrinter::doFinalization(Module &M) {
  const TargetData *TD = TM.getTargetData();

  // Print out module-level global variables here.
  for (Module::const_global_iterator I = M.global_begin(), E = M.global_end();
       I != E; ++I)
    if (I->hasInitializer()) {   // External global require no code
      // Check to see if this is a special global used by LLVM, if so, emit it.
      if (EmitSpecialLLVMGlobal(I))
        continue;
      
      O << "\n\n";
      std::string name = Mang->getValueName(I);
      Constant *C = I->getInitializer();
      unsigned Size = TD->getABITypeSize(C->getType());
      unsigned Align = TD->getPreferredAlignment(I);

      if (C->isNullValue() &&
          (I->hasLinkOnceLinkage() || I->hasInternalLinkage() ||
           I->hasWeakLinkage() /* FIXME: Verify correct */)) {
        SwitchToDataSection(".data", I);
        if (I->hasInternalLinkage())
          O << "\t.local " << name << "\n";

        O << "\t.comm " << name << "," << TD->getABITypeSize(C->getType())
          << "," << Align;
        O << "\n";
      } else {
        switch (I->getLinkage()) {
        case GlobalValue::LinkOnceLinkage:
        case GlobalValue::WeakLinkage:   // FIXME: Verify correct for weak.
          // Nonnull linkonce -> weak
          O << "\t.weak " << name << "\n";
          SwitchToDataSection("", I);
          O << "\t.section\t\".llvm.linkonce.d." << name
            << "\",\"aw\",@progbits\n";
          break;

        case GlobalValue::AppendingLinkage:
          // FIXME: appending linkage variables should go into a section of
          // their name or something.  For now, just emit them as external.
        case GlobalValue::ExternalLinkage:
          // If external or appending, declare as a global symbol
          O << "\t.globl " << name << "\n";
          // FALL THROUGH
        case GlobalValue::InternalLinkage:
          if (C->isNullValue())
            SwitchToDataSection(".bss", I);
          else
            SwitchToDataSection(".data", I);
          break;
        case GlobalValue::GhostLinkage:
          cerr << "Should not have any unmaterialized functions!\n";
          abort();
        case GlobalValue::DLLImportLinkage:
          cerr << "DLLImport linkage is not supported by this target!\n";
          abort();
        case GlobalValue::DLLExportLinkage:
          cerr << "DLLExport linkage is not supported by this target!\n";
          abort();
        default:
          assert(0 && "Unknown linkage type!");          
        }

        O << "\t.align " << Align << "\n";
        O << "\t.type " << name << ",#object\n";
        O << "\t.size " << name << "," << Size << "\n";
        O << name << ":\n";
        EmitGlobalConstant(C);
      }
    }

  return AsmPrinter::doFinalization(M);
}
