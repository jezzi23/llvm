//===-- llvm/CodeGen/GlobalISel/MachineIRBuilder.h - MIBuilder --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file declares the MachineIRBuilder class.
/// This is a helper class to build MachineInstr.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_GLOBALISEL_MACHINEIRBUILDER_H
#define LLVM_CODEGEN_GLOBALISEL_MACHINEIRBUILDER_H

#include "llvm/CodeGen/GlobalISel/Types.h"

#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/LowLevelType.h"
#include "llvm/IR/DebugLoc.h"

namespace llvm {

// Forward declarations.
class MachineFunction;
class MachineInstr;
class TargetInstrInfo;

/// Helper class to build MachineInstr.
/// It keeps internally the insertion point and debug location for all
/// the new instructions we want to create.
/// This information can be modify via the related setters.
class MachineIRBuilder {
  /// MachineFunction under construction.
  MachineFunction *MF;
  /// Information used to access the description of the opcodes.
  const TargetInstrInfo *TII;
  /// Debug location to be set to any instruction we create.
  DebugLoc DL;

  /// Fields describing the insertion point.
  /// @{
  MachineBasicBlock *MBB;
  MachineInstr *MI;
  bool Before;
  /// @}

  const TargetInstrInfo &getTII() {
    assert(TII && "TargetInstrInfo is not set");
    return *TII;
  }

  static void addRegs(MachineInstrBuilder &MIB) {}

  template <typename... MoreRegs>
  static void addRegs(MachineInstrBuilder &MIB, unsigned Reg,
                      MoreRegs... Regs) {
    MIB.addReg(Reg);
    addRegs(MIB, Regs...);
  }


public:
  /// Getter for the function we currently build.
  MachineFunction &getMF() {
    assert(MF && "MachineFunction is not set");
    return *MF;
  }

  /// Getter for the basic block we currently build.
  MachineBasicBlock &getMBB() {
    assert(MBB && "MachineBasicBlock is not set");
    return *MBB;
  }

  /// Current insertion point for new instructions.
  MachineBasicBlock::iterator getInsertPt();

  /// Setters for the insertion point.
  /// @{
  /// Set the MachineFunction where to build instructions.
  void setMF(MachineFunction &);

  /// Set the insertion point to the beginning (\p Beginning = true) or end
  /// (\p Beginning = false) of \p MBB.
  /// \pre \p MBB must be contained by getMF().
  void setMBB(MachineBasicBlock &MBB, bool Beginning = false);

  /// Set the insertion point to before (\p Before = true) or after
  /// (\p Before = false) \p MI.
  /// \pre MI must be in getMF().
  void setInstr(MachineInstr &MI, bool Before = true);
  /// @}

  /// Set the debug location to \p DL for all the next build instructions.
  void setDebugLoc(const DebugLoc &DL) { this->DL = DL; }

  /// Build and insert <empty> = \p Opcode [ { \p Tys } ] <empty>.
  /// \p Ty is the type of the instruction if \p Opcode describes
  /// a generic machine instruction. \p Ty must be LLT{} if \p Opcode
  /// does not describe a generic instruction.
  /// The insertion point is the one set by the last call of either
  /// setBasicBlock or setMI.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  /// \pre Ty == LLT{} or isPreISelGenericOpcode(Opcode)
  ///
  /// \return The newly created instruction.
  MachineInstr *buildInstr(unsigned Opcode, ArrayRef<LLT> Tys);

  /// Build and insert \p Res = \p Opcode [\p Ty] \p Uses....
  /// \p Ty is the type of the instruction if \p Opcode describes
  /// a generic machine instruction. \p Ty must be LLT{} if \p Opcode
  /// does not describe a generic instruction.
  /// The insertion point is the one set by the last call of either
  /// setBasicBlock or setMI.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  /// \pre Ty == LLT{} or isPreISelGenericOpcode(Opcode)
  ///
  /// \return The newly created instruction.
  template <typename... MoreRegs>
  MachineInstr *buildInstr(unsigned Opcode, ArrayRef<LLT> Tys, unsigned Res,
                           MoreRegs... Uses) {
    MachineInstr *NewMI = buildInstr(Opcode, Tys);
    MachineInstrBuilder MIB{getMF(), NewMI};
    MIB.addReg(Res, RegState::Define);
    addRegs(MIB, Uses...);

    return NewMI;
  }

  /// Build and insert <empty> = \p Opcode <empty>.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  /// \pre not isPreISelGenericOpcode(\p Opcode)
  ///
  /// \return The newly created instruction.
  MachineInstr *buildInstr(unsigned Opcode) {
    return buildInstr(Opcode, ArrayRef<LLT>());
  }

  /// Build and insert \p Res = \p Opcode \p Uses....
  /// The insertion point is the one set by the last call of either
  /// setBasicBlock or setMI.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  ///
  /// \return The newly created instruction.
  template <typename... MoreRegs>
  MachineInstr *buildInstr(unsigned Opcode, unsigned Res, MoreRegs... Uses) {
    return buildInstr(Opcode, ArrayRef<LLT>(), Res, Uses...);
  }

  /// Build and insert \p Res<def> = G_FRAME_INDEX \p Ty \p Idx
  ///
  /// G_FRAME_INDEX materializes the address of an alloca value or other
  /// stack-based object.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  ///
  /// \return The newly created instruction.
  MachineInstr *buildFrameIndex(LLT Ty, unsigned Res, int Idx);

  /// Build and insert \p Res<def> = G_ADD \p Ty \p Op0, \p Op1
  ///
  /// G_ADD sets \p Res to the sum of integer parameters \p Op0 and \p Op1,
  /// truncated to their width.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  ///
  /// \return The newly created instruction.
  MachineInstr *buildAdd(LLT Ty, unsigned Res, unsigned Op0, unsigned Op1);

  /// Build and insert G_BR unsized \p Dest
  ///
  /// G_BR is an unconditional branch to \p Dest.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  ///
  /// \return The newly created instruction.
  MachineInstr *buildBr(MachineBasicBlock &BB);

  /// Build and insert \p Res<def> = COPY Op
  ///
  /// Register-to-register COPY sets \p Res to \p Op.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  ///
  /// \return The newly created instruction.
  MachineInstr *buildCopy(unsigned Res, unsigned Op);

  /// Build and insert `Res0<def>, ... = G_EXTRACT Ty Src, Idx0, ...`.
  ///
  /// If \p Ty has size N bits, G_EXTRACT sets \p Res[0] to bits `[Idxs[0],
  /// Idxs[0] + N)` of \p Src and similarly for subsequent bit-indexes.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  ///
  /// \return The newly created instruction.
  MachineInstr *buildExtract(LLT Ty, ArrayRef<unsigned> Results, unsigned Src,
                             ArrayRef<unsigned> Indexes);

  /// Build and insert \p Res<def> = G_SEQUENCE \p Ty \p Ops[0], ...
  ///
  /// G_SEQUENCE concatenates each element in Ops into a single register, where
  /// Ops[0] starts at bit 0 of \p Res.
  ///
  /// \pre setBasicBlock or setMI must have been called.
  /// \pre The sum of the input sizes must equal the result's size.
  ///
  /// \return The newly created instruction.
  MachineInstr *buildSequence(LLT Ty, unsigned Res, ArrayRef<unsigned> Ops);
};

} // End namespace llvm.
#endif // LLVM_CODEGEN_GLOBALISEL_MACHINEIRBUILDER_H
