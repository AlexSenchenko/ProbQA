#include "stdafx.h"
#include "PqaEngineBaseFactory.h"
#include "../PqaCore/Interface/PqaErrorParams.h"
#include "../PqaCore/CpuEngine.h"
#include "../PqaCore/DoubleNumber.h"

using namespace SRPlat;

namespace ProbQA {

IPqaEngine* PqaEngineBaseFactory::CreateCpuEngine(PqaError& err, const EngineDefinition& engDef) {
  try {
    std::unique_ptr<IPqaEngine> pEngine;
    switch (engDef._prec._type) {
    case TPqaPrecisionType::Double:
      pEngine.reset(new CpuEngine<DoubleNumber>(engDef));
      break;
    default:
      //TODO: implement
      err = PqaError(PqaErrorCode::NotImplemented, new NotImplementedErrorParams(SRString::MakeUnowned(
        "ProbQA Engine on CPU for precision except double.")));
      return nullptr;
    }
    err.Release();
    return pEngine.release();
  }
  catch (SRException &ex) {
    err.SetFromException(std::move(ex));
  }
  catch (std::exception& ex) {
    err.SetFromException(ex);
  }
  return nullptr;
}

IPqaEngine* PqaEngineBaseFactory::CreateCudaEngine(PqaError& err, const EngineDefinition& engDef) {
  //TODO: implement
  err = PqaError(PqaErrorCode::NotImplemented, new NotImplementedErrorParams(SRString::MakeUnowned(
    "ProbQA Engine on CUDA.")));
  return nullptr;
}

IPqaEngine* PqaEngineBaseFactory::CreateGridEngine(PqaError& err, const EngineDefinition& engDef) {
  //TODO: implement
  err = PqaError(PqaErrorCode::NotImplemented, new NotImplementedErrorParams(SRString::MakeUnowned(
    "ProbQA Engine over a grid.")));
  return nullptr;
}

} // namespace ProbQA
