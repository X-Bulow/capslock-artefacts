#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
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
                    if (Name != "capslock-function-entry") {
                        return false;
                    }
                    MPM.addPass(CapsLockFunctionEntryPass());
                    return true;
                   }
            );
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getCapsLockPassPluginInfo();
}
