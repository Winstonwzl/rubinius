//===-- X86TargetMachine.h - Define TargetMachine for the X86 ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares the X86 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef X86TARGETMACHINE_H
#define X86TARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetFrameInfo.h"
#include "X86.h"
#include "X86ELFWriterInfo.h"
#include "X86InstrInfo.h"
#include "X86JITInfo.h"
#include "X86Subtarget.h"
#include "X86ISelLowering.h"

namespace llvm {

class X86TargetMachine : public LLVMTargetMachine {
  X86Subtarget      Subtarget;
  const TargetData  DataLayout; // Calculates type size & alignment
  TargetFrameInfo   FrameInfo;
  X86InstrInfo      InstrInfo;
  X86JITInfo        JITInfo;
  X86TargetLowering TLInfo;
  X86ELFWriterInfo  ELFWriterInfo;
  Reloc::Model      DefRelocModel; // Reloc model before it's overridden.

protected:
  virtual const TargetAsmInfo *createTargetAsmInfo() const;
  
public:
  X86TargetMachine(const Module &M, const std::string &FS, bool is64Bit);

  virtual const X86InstrInfo     *getInstrInfo() const { return &InstrInfo; }
  virtual const TargetFrameInfo  *getFrameInfo() const { return &FrameInfo; }
  virtual       TargetJITInfo    *getJITInfo()         { return &JITInfo; }
  virtual const TargetSubtarget  *getSubtargetImpl() const{ return &Subtarget; }
  virtual       X86TargetLowering *getTargetLowering() const { 
    return const_cast<X86TargetLowering*>(&TLInfo); 
  }
  virtual const TargetRegisterInfo    *getRegisterInfo() const {
    return &InstrInfo.getRegisterInfo();
  }
  virtual const TargetData       *getTargetData() const { return &DataLayout; }
  virtual const X86ELFWriterInfo *getELFWriterInfo() const {
    return Subtarget.isTargetELF() ? &ELFWriterInfo : 0;
  }

  static unsigned getModuleMatchQuality(const Module &M);
  static unsigned getJITMatchQuality();
  
  // Set up the pass pipeline.
  virtual bool addInstSelector(PassManagerBase &PM, bool Fast);
  virtual bool addPreRegAlloc(PassManagerBase &PM, bool Fast);
  virtual bool addPostRegAlloc(PassManagerBase &PM, bool Fast);
  virtual bool addAssemblyEmitter(PassManagerBase &PM, bool Fast, 
                                  std::ostream &Out);
  virtual bool addCodeEmitter(PassManagerBase &PM, bool Fast,
                              bool DumpAsm, MachineCodeEmitter &MCE);
  virtual bool addSimpleCodeEmitter(PassManagerBase &PM, bool Fast,
                                    bool DumpAsm, MachineCodeEmitter &MCE);
};

/// X86_32TargetMachine - X86 32-bit target machine.
///
class X86_32TargetMachine : public X86TargetMachine {
public:
  X86_32TargetMachine(const Module &M, const std::string &FS);
  
  static unsigned getJITMatchQuality();
  static unsigned getModuleMatchQuality(const Module &M);
};

/// X86_64TargetMachine - X86 64-bit target machine.
///
class X86_64TargetMachine : public X86TargetMachine {
public:
  X86_64TargetMachine(const Module &M, const std::string &FS);
  
  static unsigned getJITMatchQuality();
  static unsigned getModuleMatchQuality(const Module &M);
};

} // End llvm namespace

#endif
