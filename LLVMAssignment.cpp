/*
 * @Author: Chendong Yu 
 * @Date: 2019-11-08 16:05:57 
 * @Last Modified by: Chendong Yu
 * @Last Modified time: 2019-11-09 16:11:41
 */
//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/User.h>
#include <llvm/Pass.h>

#include <llvm/Support/raw_ostream.h>

#include <vector>
#include <set>
#include <map>
#include <string>
#include <algorithm>
#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif
using namespace llvm;
#if LLVM_VERSION_MAJOR >= 4
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
#endif
/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang will
 * have optnone attribute which would lead to some transform passes disabled, like mem2reg.
 */
#if LLVM_VERSION_MAJOR == 5
struct EnableFunctionOptPass : public FunctionPass
{
  static char ID;
  EnableFunctionOptPass() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override
  {
    if (F.hasFnAttribute(Attribute::OptimizeNone))
    {
      F.removeFnAttr(Attribute::OptimizeNone);
    }
    return true;
  }
};

char EnableFunctionOptPass::ID = 0;
#endif

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
///Updated 11/10/2017 by fargo: make all functions
///processed by mem2reg before this pass.
struct FuncPtrPass : public ModulePass
{
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}

  std::map<int, std::vector<std::string>> results;
  std::vector<std::string> funcNames;

  void PrintResult()
  {
    for (auto ii = results.begin(), ie = results.end(); ii != ie; ii++)
    {
      if (ii->second.size() == 0)
      {
        continue;
      }
      errs() << ii->first << " : ";
      for (auto ji = ii->second.begin(), je = ii->second.end() - 1; ji != je; ji++)
      {
        errs() << *ji << ", ";
      }
      errs() << *(ii->second.end() - 1) << "\n";
    }
  }

  void Push(std::string funcname)
  {
    if (find(funcNames.begin(), funcNames.end(), funcname) == funcNames.end())
    {
      funcNames.push_back(funcname);
    }
  }

  void HandlePHINode(PHINode *phiNode)
  {
    for (Value *value : phiNode->incoming_values())
    {
      if (auto func = dyn_cast<Function>(value))
      {
        Push(func->getName());
      }
      else if (auto phiNode = dyn_cast<PHINode>(value))
      {
        HandlePHINode(phiNode);
      }
      else if (auto argument = dyn_cast<Argument>(value))
      {
        HandleArgument(argument);
      }
    }
  }

  void HandleArgument(Argument *argument)
  {
    unsigned int arg_index = argument->getArgNo();
    Function *funcParent = argument->getParent();
    errs() << funcParent->getName() << "\n";
    for (User *user : funcParent->users())
    {
      if (CallInst *callInst = dyn_cast<CallInst>(user))
      {
        // if argument at 3 , then foo(arg1,arg2) will be pass
        if (arg_index + 1 <= callInst->getNumArgOperands())
        {
          errs() << "here"
                 << "\n";
          Value *value = callInst->getArgOperand(arg_index);
          if (callInst->getCalledFunction() != funcParent)
          { // 递归问题
            Function *func = callInst->getCalledFunction();

            errs() << "here1"
                   << "\n";
          }
          else if (PHINode *phiNode = dyn_cast<PHINode>(value))
          {
            errs() << "here2"
                   << "\n";
            HandlePHINode(phiNode);
          }
          else if (Function *func = dyn_cast<Function>(value))
          {
            errs() << "here3"
                   << "\n";
            Push(func->getName());
          }else if(Argument *argument = dyn_cast<Argument>(value)){
            errs() << "here4" << "\n";
            HandleArgument(argument);
          }
        }
      }
    }
  }

  void GetResult(CallInst *callInst)
  {
    // callinst
    Function *func = callInst->getCalledFunction();
    int line = callInst->getDebugLoc().getLine();
    errs()<<"lines:"<<line<<"\n";
    funcNames.clear();
    if (func)
    { // calledFunction exits
      /*
      line:1	llvm.dbg.value
      */
      std::string funcname = func->getName();
      if (!(funcname == std::string("llvm.dbg.value")))
      {
        Push(funcname);
        results.insert(std::pair<int, std::vector<std::string>>(line, funcNames));
      }
    }
    else
    { //calledFunction doesn't exits
      /// Return the value actually being called or invoked.
      Value *value = callInst->getCalledValue();
      if (PHINode *phiNode = dyn_cast<PHINode>(value))
      {
        HandlePHINode(phiNode);
      }
      else if (Argument *argument = dyn_cast<Argument>(value))
      {
        HandleArgument(argument);
      }else if(CallInst *callInst = dyn_cast<CallInst>(value)){
        errs()<<"I am in"<< "\n";
      }
      results.insert(std::pair<int, std::vector<std::string>>(line, funcNames));
    }
  }

  bool runOnModule(Module &M) override
  {
    errs() << "Hello: ";
    errs().write_escaped(M.getName()) << '\n';
    //M.dump();
    //M.print(llvm::errs(), nullptr);
    //errs() << "------------------------------\n";
    //for function in Module
    for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; fi++)
    {
      // for basicblock in function
      for (Function::iterator bi = fi->begin(), be = fi->end(); bi != be; bi++)
      {
        // for instruction in basicblock
        for (BasicBlock::iterator ii = bi->begin(), ie = bi->end(); ii != ie; ii++)
        {
          Instruction *inst = dyn_cast<Instruction>(ii);
          if (CallInst *callInst = dyn_cast<CallInst>(inst))
          {
            GetResult(callInst);
          }
        }
      }
    }
    PrintResult();
    return false;
  }
};

char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass", "Print function call instruction");

static cl::opt<std::string>
    InputFilename(cl::Positional,
                  cl::desc("<filename>.bc"),
                  cl::init(""));

int main(int argc, char **argv)
{
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  // Parse the command line to read the Inputfilename
  cl::ParseCommandLineOptions(argc, argv,
                              "FuncPtrPass \n My first LLVM too which does not do much.\n");

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M)
  {
    Err.print(argv[0], errs());
    return 1;
  }

  llvm::legacy::PassManager Passes;

///Remove functions' optnone attribute in LLVM5.0
#if LLVM_VERSION_MAJOR == 5
  Passes.add(new EnableFunctionOptPass());
#endif
  ///Transform it to SSA
  Passes.add(llvm::createPromoteMemoryToRegisterPass());

  /// Your pass to print Function and Call Instructions
  Passes.add(new FuncPtrPass());
  Passes.run(*M.get());
}
