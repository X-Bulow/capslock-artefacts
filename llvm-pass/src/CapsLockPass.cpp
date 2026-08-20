#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

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

    static bool isRequired() {
        return true;
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
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return getCapsLockPassPluginInfo();
}
