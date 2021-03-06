// Probabilistic Question-Answering system
// @2017 Sarge Rogatch
// This software is distributed under GNU AGPLv3 license. See file LICENSE in repository root for details.

#pragma once

#include "../PqaCore/BaseCpuEngine.fwd.h"
#include "../PqaCore/CEQuiz.fwd.h"
#include "../PqaCore/CEBaseTask.fwd.h"
#include "../PqaCore/CETask.fwd.h"
#include "../PqaCore/Interface/IPqaEngine.h"
#include "../PqaCore/GapTracker.h"
#include "../PqaCore/MaintenanceSwitch.h"
#include "../PqaCore/Interface/PqaCommon.h"

namespace ProbQA {

class BaseCpuEngine : public IPqaEngine {
public: // constants
  static constexpr size_t _cMemPoolMaxSimds = size_t(1) << 10;
  static constexpr size_t _cFileBufSize = size_t(1024) * 1024;

public: // types
  typedef SRPlat::SRMemPool<SRPlat::SRSimd::_cLogNBits, _cMemPoolMaxSimds> TMemPool;

private:
  const SRPlat::SRThreadCount _nLooseWorkers;

protected: // variables
  TMemPool _memPool; // thread-safe itself
  SRPlat::SRThreadPool _tpWorkers; // thread-safe itself

  const PrecisionDefinition _precDef;
  EngineDimensions _dims; // Guarded by _rws in maintenance mode. Read-only in regular mode.
  const SRPlat::SRThreadCount _nMemOpThreads;
  std::atomic<uint64_t> _nQuestionsAsked = 0;

  //// Don't violate the order of obtaining these locks, so to avoid a deadlock.
  //// Actually the locks form directed acyclic graph indicating which locks must be obtained one after another.
  //// However, to simplify the code we list them here topologically sorted.
  MaintenanceSwitch _maintSwitch; // regular/maintenance mode switch
  SRPlat::SRReaderWriterSync _rws; // KB read-write
  SRPlat::SRCriticalSection _csQuizReg; // quiz registry

  GapTracker<TPqaId> _quizGaps; // Guarded by _csQuizReg

  GapTracker<TPqaId> _questionGaps; // Guarded by _rws in maintenance mode. Read-only in regular mode.
  GapTracker<TPqaId> _targetGaps; // Guarded by _rws in maintenance mode. Read-only in regular mode.

  //// Cache-insensitive data
  std::atomic<SRPlat::ISRLogger*> _pLogger;

protected: // methods
  static SRPlat::SRThreadCount CalcMemOpThreads();

  explicit BaseCpuEngine(const EngineDefinition& engDef, const size_t workerStackSize);

  TPqaId FindNearestQuestion(const TPqaId iMiddle, const CEBaseQuiz &quiz);

public: // Internal interface methods
  SRPlat::ISRLogger *GetLogger() const { return _pLogger.load(std::memory_order_relaxed); }
  TMemPool& GetMemPool() { return _memPool; }
  SRPlat::SRThreadPool& GetWorkers() { return _tpWorkers; }
  SRPlat::SRReaderWriterSync& GetRws() { return _rws; }

  const EngineDimensions& GetDims() const override { return _dims; }
  const GapTracker<TPqaId>& GetQuestionGaps() const { return _questionGaps; }
  const GapTracker<TPqaId>& GetTargetGaps() const { return _targetGaps; }

  const SRPlat::SRThreadCount GetNLooseWorkers() const { return _nLooseWorkers; }
};

} // namespace ProbQA
