#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "capslock-function-entry"

using namespace llvm;

namespace {

class CapsLockHelloPass : public PassInfoMixin<CapsLockHelloPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        (void)AM;
        if (!F.isDeclaration()) {
            errs() << "capslock-function: " << F.getName() << '\n';
        }
        return PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

class CapsLockFunctionEntryPass : public PassInfoMixin<CapsLockFunctionEntryPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        (void)AM;
        LLVMContext &Context = M.getContext();
        FunctionType *HookType = FunctionType::get(Type::getVoidTy(Context), {PointerType::getUnqual(Context)}, false);
        FunctionCallee Hook = M.getOrInsertFunction("capslock_function_entry", HookType);

        bool changed = false;
        for (Function &F : M) {
            if (F.isDeclaration() || F.getName() == "capslock_function_entry")
                continue;
            LLVM_DEBUG(
                dbgs() << "capslock-function-entry: instrumenting "
                    << F.getName() << '\n'
            );
            IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());
            Value *FunctionName = Builder.CreateGlobalStringPtr(F.getName());
            Builder.CreateCall(Hook, {FunctionName});
            changed = true;
        }
        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
    static bool isRequired() { return true; }
};

class CapsLockMemoryAccessPass : public PassInfoMixin<CapsLockMemoryAccessPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
        (void)AM;
        const DataLayout &DL = M.getDataLayout();

        for (Function &F : M) {
            if (F.isDeclaration())
                continue;
            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {
                    visitInstruction(F, DL, I);
                }
            }
        }
        return PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }

private:
    static void visitInstruction(const Function &F, const DataLayout &DL, Instruction &I) {
        if (auto *LI = dyn_cast<LoadInst>(&I)) {
            reportAccess(F, DL, "load", LI->getType(), LI->getPointerOperand());
            return;
        }
        if (auto *SI = dyn_cast<StoreInst>(&I)) {
            reportAccess(F, DL, "store", SI->getValueOperand()->getType(), SI->getPointerOperand());
            return;
        }
        // These categories also read or write memory but are not yet
        // characterised by this B4 prototype; they are recorded explicitly
        // rather than silently skipped so the gap stays visible until M-C.
        if (isa<AtomicRMWInst>(&I)) {
            reportUnhandled(F, "atomicrmw");
            return;
        }
        if (isa<AtomicCmpXchgInst>(&I)) {
            reportUnhandled(F, "cmpxchg");
            return;
        }
        if (isa<MemCpyInst>(&I)) {
            reportUnhandled(F, "llvm.memcpy");
            return;
        }
        if (isa<MemMoveInst>(&I)) {
            reportUnhandled(F, "llvm.memmove");
            return;
        }
        if (isa<MemSetInst>(&I)) {
            reportUnhandled(F, "llvm.memset");
            return;
        }
        // AtomicMemCpyInst etc. are a separate class hierarchy from
        // MemCpyInst (MemTransferBase<AtomicMemIntrinsic> vs.
        // MemTransferBase<MemIntrinsic>), so the isa<> checks above do not
        // catch them; they need their own checks.
        if (isa<AtomicMemCpyInst>(&I)) {
            reportUnhandled(F, "llvm.memcpy.element.unordered.atomic");
            return;
        }
        if (isa<AtomicMemMoveInst>(&I)) {
            reportUnhandled(F, "llvm.memmove.element.unordered.atomic");
            return;
        }
        if (isa<AtomicMemSetInst>(&I)) {
            reportUnhandled(F, "llvm.memset.element.unordered.atomic");
            return;
        }
        if (isa<VAArgInst>(&I)) {
            reportUnhandled(F, "va_arg");
            return;
        }
        if (auto *II = dyn_cast<IntrinsicInst>(&I)) {
            switch (II->getIntrinsicID()) {
            case Intrinsic::masked_load:
                reportUnhandled(F, "llvm.masked.load");
                return;
            case Intrinsic::masked_store:
                reportUnhandled(F, "llvm.masked.store");
                return;
            case Intrinsic::masked_gather:
                reportUnhandled(F, "llvm.masked.gather");
                return;
            case Intrinsic::masked_scatter:
                reportUnhandled(F, "llvm.masked.scatter");
                return;
            default:
                break;
            }
        }
    }

    static void reportAccess(
        const Function &F,
        const DataLayout &DL,
        StringRef Kind,
        Type *AccessedTy,
        const Value *Ptr) {
        TypeSize Size = DL.getTypeStoreSize(AccessedTy);
        errs() << "capslock-memory-access: " << F.getName() << ": " << Kind
               << ' ' << *AccessedTy << ", size=" << Size.getFixedValue()
               << " bytes, ptr=";
        Ptr->printAsOperand(errs(), /*PrintType=*/false);
        errs() << '\n';
    }

    static void reportUnhandled(const Function &F, StringRef Kind) {
        errs() << "capslock-memory-access: " << F.getName() << ": " << Kind
               << " access not yet handled by this B4 prototype\n";
    }
};

} // namespace

PassPluginLibraryInfo getCapsLockPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "CapsLockPass",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name != "capslock-hello") {
                        return false;
                    }

                    FPM.addPass(CapsLockHelloPass());
                    return true;
                   }
            );

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "capslock-function-entry") {
                        MPM.addPass(CapsLockFunctionEntryPass());
                        return true;
                    }
                    if (Name == "capslock-memory-access") {
                        MPM.addPass(CapsLockMemoryAccessPass());
                        return true;
                    }
                    return false;
                   }
            );
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getCapsLockPassPluginInfo();
}
