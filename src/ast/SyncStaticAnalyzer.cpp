#include "SyncStaticAnalyzer.h"

#include "common/SyncStaticAnalyzerImpl.h"

void SyncStaticAnalyzer::Analyze(ASTNode* root)
{
	SyncStaticAnalyzerDetail::Analyzer analyzer;
	analyzer.Analyze(root);
}
