#include "cntrl-flow-domain-funtions.h"


ControlFlowDomainExprStmtNode* mapExprToControlflowDst(Expr* expr)
{
	if (expr == nullptr)
		return nullptr;

	return new ControlFlowDomainExprStmtNode(expr);
}

ControlFlowDomainFuncDeclNode* mapToControlflowDst(FunctionDecl* funcDecl)
{
	if (funcDecl == nullptr)
		return nullptr;

	auto body = mapToControlflowDst(funcDecl->getBody());
	return new ControlFlowDomainFuncDeclNode(funcDecl, body);
}

ControlFlowDomainStmtNode* mapToControlflowDst(Stmt* stmt)
{
	if (stmt == nullptr)
		return nullptr;

	if (auto compoundStmt = dyn_cast<clang::CompoundStmt>(stmt))
	{
		vector<ControlFlowDomainStmtNode*> childs;
		for (auto stmt : compoundStmt->body())
		{
			childs.push_back(mapToControlflowDst(stmt));
		}
		return new ControlFlowDomainStmtListNode(compoundStmt, childs);
	}		
	if (auto declStmt = dyn_cast<clang::DeclStmt>(stmt))
	{
		bool isAllVarDecl = true;
		for (auto childDecl : declStmt->getDeclGroup())
		{
			isAllVarDecl = isAllVarDecl && isa<clang::VarDecl>(childDecl);
		}
		if (isAllVarDecl)
			return new ControlFlowDomainVarDeclStmtNode(declStmt);
	}
	if (auto ifStmt = dyn_cast<clang::IfStmt>(stmt))
	{
		auto currentIf = ifStmt;
		vector<ControlFlowDomainIfStmtPart*> ifParts;
		do
		{
			ifParts.push_back(new ControlFlowDomainIfStmtPart(currentIf, mapExprToControlflowDst(currentIf->getCond()), mapToControlflowDst(currentIf->getThen())));
		} while (currentIf->getElse() && isa<clang::IfStmt>(currentIf->getElse()) && (currentIf = dyn_cast<clang::IfStmt>(currentIf->getElse())));
		auto _else = mapToControlflowDst(currentIf->getElse());
		return new ControlFlowDomainIfStmtNode(ifParts, _else);
	}
	if (auto whileStmt = dyn_cast<clang::WhileStmt>(stmt))
	{
		auto expr = mapExprToControlflowDst(whileStmt->getCond());
		auto body = mapToControlflowDst(whileStmt->getBody());
		return new ControlFlowDomainWhileStmtNode(whileStmt, expr, body);
	}
	if (auto doStmt = dyn_cast<clang::DoStmt>(stmt))
	{
		auto expr = mapExprToControlflowDst(doStmt->getCond());
		auto body = mapToControlflowDst(doStmt->getBody());
		return new ControlFlowDomainDoWhileStmtNode(doStmt, expr, body);
	}
	if (auto returnStmt = dyn_cast<clang::ReturnStmt>(stmt))
	{
		auto expr = mapExprToControlflowDst(returnStmt->getRetValue());
		return new ControlFlowDomainReturnStmtNode(returnStmt, expr);
	}
	if (auto exprStmt = dyn_cast<clang::Expr>(stmt))
	{
		return mapExprToControlflowDst(exprStmt);
	}

	return new ControlFlowDomainUndefinedStmtNode(stmt);
}

std::string toOriginalCppString(ControlFlowDomainFuncDeclNode* func, clang::SourceManager& mgr)
{
	return get_source_text(func->getAstNode()->getSourceRange(), mgr);
}


std::stringstream& printTabs(std::stringstream& ss, int level)
{
	ss << string(level * 4, ' ');
	return ss;
}

string getAstRawString(Stmt* node, clang::SourceManager& mgr)
{
	auto s = removeMultipleSpaces(removeNewLines(get_source_text(node->getSourceRange(), mgr)));
	if (s.length() == 0)
	{
		s = removeMultipleSpaces(removeNewLines(get_source_text_raw_tr(node->getSourceRange(), mgr)));
	}
	return s;
}


std::stringstream& printExpr(std::stringstream& ss, ControlFlowDomainExprStmtNode* expr, clang::SourceManager& mgr)
{
	if (expr == nullptr || expr->getAstNode() == nullptr)
		return ss;

	auto s = removeMultipleSpaces(removeNewLines(get_source_text(expr->getAstNode()->getSourceRange(), mgr)));
	if (s.length() == 0)
	{
		s = removeMultipleSpaces(removeNewLines(get_source_text_raw_tr(expr->getAstNode()->getSourceRange(), mgr)));
	}

	ss << s;
	return ss;
}


void toCustomCppStringInner(std::stringstream& ss, ControlFlowDomainStmtNode* stmt, clang::SourceManager& mgr, bool isDebug, int recursionLevel = 0, bool ignoreFirstLineSpaces = false)
{
	if (stmt == nullptr)
		return;


	if (auto node = dynamic_cast<ControlFlowDomainUndefinedStmtNode*>(stmt))
	{
		if (isDebug)
		{
			if (!ignoreFirstLineSpaces)
				printTabs(ss, recursionLevel);
			ss << "/* undefined stmt */" << "\n";
		}
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainStmtListNode*>(stmt))
	{
		if (!ignoreFirstLineSpaces)
			printTabs(ss, recursionLevel);
		ss << "{" << "\n";
		for (auto innerStmt : node->getChilds())
		{
			toCustomCppStringInner(ss, innerStmt, mgr, isDebug, recursionLevel + 1);
		}
		printTabs(ss, recursionLevel) << "}" << "\n";
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainExprStmtNode*>(stmt))
	{
		if (!ignoreFirstLineSpaces)
			printTabs(ss, recursionLevel);
		printExpr(ss, node, mgr);
		ss << ";\n";
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainIfStmtNode*>(stmt))
	{
		auto ifParts = node->getIfParts();
		for (int i = 0; i < ifParts.size(); ++i)
		{
			if (i == 0)
			{
				if (!ignoreFirstLineSpaces)
					printTabs(ss, recursionLevel);

				ss << "if (";
				printExpr(ss, ifParts[i]->getExpr(), mgr);
				ss << ")\n";
			}
			else 
			{
				printTabs(ss, recursionLevel);
				ss << "else if (";
				printExpr(ss, ifParts[i]->getExpr(), mgr);
				ss << ")\n";
			}
			toCustomCppStringInner(ss, ifParts[i]->getThenBody(), mgr, isDebug, dynamic_cast<ControlFlowDomainStmtListNode*>(ifParts[i]->getThenBody()) ? recursionLevel : recursionLevel + 1);
		}
		if (node->getElseBody() && node->getElseBody()->getAstNode())
		{			
			printTabs(ss, recursionLevel) << "else\n";
			toCustomCppStringInner(ss, node->getElseBody(), mgr, isDebug, dynamic_cast<ControlFlowDomainStmtListNode*>(node->getElseBody()) ? recursionLevel : recursionLevel + 1);
		}
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainWhileStmtNode*>(stmt))
	{
		if (!ignoreFirstLineSpaces)
			printTabs(ss, recursionLevel);
		ss << "while (";
		printExpr(ss, node->getExpr(), mgr);
		ss << ")\n";
		toCustomCppStringInner(ss, node->getBody(), mgr, isDebug, dynamic_cast<ControlFlowDomainStmtListNode*>(node->getBody()) ? recursionLevel : recursionLevel + 1);
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainDoWhileStmtNode*>(stmt))
	{
		if (!ignoreFirstLineSpaces)
			printTabs(ss, recursionLevel);
		ss << "do\n";
		toCustomCppStringInner(ss, node->getBody(), mgr, isDebug, dynamic_cast<ControlFlowDomainStmtListNode*>(node->getBody()) ? recursionLevel : recursionLevel + 1);
		printTabs(ss, recursionLevel) << "while (";
		printExpr(ss, node->getExpr(), mgr);
		ss << ");\n";
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainReturnStmtNode*>(stmt))
	{
		if (!ignoreFirstLineSpaces)
			printTabs(ss, recursionLevel);
		if (node->getExpr() == nullptr || node->getExpr()->getAstNode() == nullptr)
		{
			ss << "return;\n";
		}
		else
		{
			ss << "return ";
			printExpr(ss, node->getExpr(), mgr);
			ss << ";\n";
		}
		return;
	}
	if (auto node = dynamic_cast<ControlFlowDomainVarDeclStmtNode*>(stmt))
	{
		if (!ignoreFirstLineSpaces)
			printTabs(ss, recursionLevel);
		ss << removeNewLines(get_source_text(node->getAstNode()->getSourceRange(), mgr));
		ss << "\n";
		return;
	}

	throw std::string("unexpected node");
}


std::string toCustomCppString(ControlFlowDomainFuncDeclNode * func, clang::SourceManager & mgr, bool isDebug)
{
	std::stringstream ss;
	//func->getAstNode()->getDeclaredReturnType().getAsString()
	ss << func->getAstNode()->getDeclaredReturnType().getAsString() << " " 
	   << func->getAstNode()->getDeclName().getAsString()
	   << "(" << removeMultipleSpaces(removeNewLines(get_source_text(func->getAstNode()->getParametersSourceRange(), mgr))) << ")\n";

	//ss << removeNewLines(get_source_text(func->getAstNode()->getSourceRange(), mgr));
	toCustomCppStringInner(ss, func->getBody(), mgr, isDebug);
	return ss.str();
}


ControlFlowDomainExprRdfNode* mapToRdfNode(ControlFlowDomainExprStmtNode* node, int& idGenerator, clang::SourceManager& mgr)
{
	return new ControlFlowDomainExprRdfNode(++idGenerator, getAstRawString(node->getAstNode(), mgr));
}

/*
ControlFlowDomainSequenceRdfNode* mapToRdfNodeToSeq(ControlFlowDomainStmtNode* node, int& idGenerator, clang::SourceManager& mgr)
{
	if (auto castedNode = dynamic_cast<ControlFlowDomainStmtListNode*>(node))
	{
		vector<ControlFlowDomainLinkedRdfNode*> body;
		ControlFlowDomainLinkedRdfNode* prev = NULL;
		for (auto child : castedNode->getChilds())
		{
			auto current = mapToRdfNode(child, idGenerator, mgr);
			if (prev)
			{
				prev->setNext(current);
			}
			body.push_back(current);
			prev = current;
		}
		if (body.size() > 0)
		{
			body[0]->setFirst();
			body[body.size() - 1]->setLast();
		}

		return new ControlFlowDomainSequenceRdfNode(++id)
	}
	if (auto castedNode = dynamic_cast<ControlFlowDomainStmtNode*>(node))
	{
		vector<ControlFlowDomainLinkedRdfNode*> body;
		ControlFlowDomainLinkedRdfNode* prev = NULL;
		for (auto child : castedNode->getChilds())
		{
			auto current = mapToRdfNode(child, idGenerator, mgr);
			if (prev)
			{
				prev->setNext(current);
			}
			body.push_back(current);
			prev = current;
		}
		if (body.size() > 0)
		{
			body[0]->setFirst();
			body[body.size() - 1]->setLast();
		}

		return new ControlFlowDomainSequenceRdfNode(++id)
	}
}

*/

ControlFlowDomainLinkedRdfNode* mapToRdfNode(ControlFlowDomainStmtNode* node, int& idGenerator, clang::SourceManager& mgr, bool forceToSeq = false)
{	
	if (node == NULL || dynamic_cast<ControlFlowDomainUndefinedStmtNode*>(node))
	{
		return NULL;
	}

	if (auto castedNode = dynamic_cast<ControlFlowDomainStmtListNode*>(node))
	{
		vector<ControlFlowDomainLinkedRdfNode*> body;
		ControlFlowDomainLinkedRdfNode* prev = NULL;
		auto stmts = castedNode->getChilds();
		ControlFlowDomainStmtNode* child;
		for (int i = 0; (i < stmts.size()) && (child = stmts[i]); ++i)
		{
			auto current = mapToRdfNode(child, idGenerator, mgr);
			if (!current)
				continue;
			if (prev)
			{
				prev->setNext(current);
			}
			current->setIndex(i);
			body.push_back(current);
			prev = current;
		}
		if (body.size() > 0)
		{
			body[0]->setFirst();
			body[body.size() - 1]->setLast();
		}

		return new ControlFlowDomainSequenceRdfNode(++idGenerator, body);
	}
	if (auto castedNode = dynamic_cast<ControlFlowDomainExprStmtNode*>(node))
	{
		auto val = new ControlFlowDomainStmtRdfNode(++idGenerator, getAstRawString(node->getAstNode(), mgr) + ";");
		if (forceToSeq)
		{
			vector<ControlFlowDomainLinkedRdfNode*> body;
			body.push_back(val);
			val->setFirst();
			val->setLast();
			val->setIndex(0);
			return new ControlFlowDomainSequenceRdfNode(++idGenerator, body);
		}		
		else
		{
			return val;
		}
	}

	if (auto castedNode = dynamic_cast<ControlFlowDomainIfStmtNode*>(node))
	{
		vector<ControlFlowDomainAlternativeBranchRdfNode*> alternatives;
		ControlFlowDomainAlternativeBranchRdfNode* prevBranchNode = NULL;
		auto ifParts = castedNode->getIfParts();
		for (int i = 0; i < ifParts.size(); ++i)
		{
			auto seq = (ControlFlowDomainSequenceRdfNode*)mapToRdfNode(ifParts[i]->getThenBody(), idGenerator, mgr, true);
			auto expr = mapToRdfNode(ifParts[i]->getExpr(), idGenerator, mgr);
			ControlFlowDomainAlternativeBranchRdfNode* branchNode = (i == 0)
				? (ControlFlowDomainAlternativeBranchRdfNode*)new ControlFlowDomainAlternativeIfBranchRdfNode(++idGenerator, expr, vector<ControlFlowDomainLinkedRdfNode*>(seq->getBody()))
				: (ControlFlowDomainAlternativeBranchRdfNode*)new ControlFlowDomainAlternativeElseIfBranchRdfNode(++idGenerator, expr, vector<ControlFlowDomainLinkedRdfNode*>(seq->getBody()));
			if (prevBranchNode)
			{
				prevBranchNode->setNext(branchNode);
			}
			branchNode->setIndex(i);
			alternatives.push_back(branchNode);
			seq->setBody(vector<ControlFlowDomainLinkedRdfNode*>());
			delete seq;
			prevBranchNode = branchNode;
		}
		if (castedNode->getElseBody() && castedNode->getElseBody()->getAstNode())
		{
			auto seq = (ControlFlowDomainSequenceRdfNode*)mapToRdfNode(castedNode->getElseBody(), idGenerator, mgr, true);
			auto branchNode = new ControlFlowDomainAlternativeElseBranchRdfNode(++idGenerator, vector<ControlFlowDomainLinkedRdfNode*>(seq->getBody()));
			prevBranchNode->setNext(branchNode);
			alternatives.push_back(branchNode);
			seq->setBody(vector<ControlFlowDomainLinkedRdfNode*>());
			delete seq;
		}
		if (alternatives.size() > 0)
		{
			alternatives[0]->setFirst();
			alternatives[alternatives.size() - 1]->setLast();
		}

		return new ControlFlowDomainAlternativeRdfNode(++idGenerator, alternatives);
	}

	if (auto castedNode = dynamic_cast<ControlFlowDomainWhileStmtNode*>(node))
	{
		auto exprRdf = mapToRdfNode(castedNode->getExpr(), idGenerator, mgr);
		auto bodyRdf = (ControlFlowDomainSequenceRdfNode*)mapToRdfNode(castedNode->getBody(), idGenerator, mgr, true);
		return new ControlFlowDomainWhileDoRdfNode(++idGenerator, exprRdf, bodyRdf);
	}
	if (auto castedNode = dynamic_cast<ControlFlowDomainDoWhileStmtNode*>(node))
	{
		auto exprRdf = mapToRdfNode(castedNode->getExpr(), idGenerator, mgr);
		auto bodyRdf = (ControlFlowDomainSequenceRdfNode*)mapToRdfNode(castedNode->getBody(), idGenerator, mgr, true);
		return new ControlFlowDomainDoWhileRdfNode(++idGenerator, exprRdf, bodyRdf);
	}
	if (auto castedNode = dynamic_cast<ControlFlowDomainReturnStmtNode*>(node))
	{
		auto val = new ControlFlowDomainStmtRdfNode(++idGenerator, getAstRawString(node->getAstNode(), mgr) + ";");
		if (forceToSeq)
		{
			vector<ControlFlowDomainLinkedRdfNode*> body;
			body.push_back(val);
			val->setFirst();
			val->setLast();
			val->setIndex(0);
			return new ControlFlowDomainSequenceRdfNode(++idGenerator, body);
		}
		else
		{
			return val;
		}
	}
	if (auto castedNode = dynamic_cast<ControlFlowDomainVarDeclStmtNode*>(node))
	{
		auto val = new ControlFlowDomainStmtRdfNode(++idGenerator, getAstRawString(node->getAstNode(), mgr));
		if (forceToSeq)
		{
			vector<ControlFlowDomainLinkedRdfNode*> body;
			body.push_back(val);
			val->setFirst();
			val->setLast();
			val->setIndex(0);
			return new ControlFlowDomainSequenceRdfNode(++idGenerator, body);
		}
		else
		{
			return val;
		}
	}

	return NULL;
}




ControlFlowDomainAlgorythmRdfNode* mapToRdfNode(ControlFlowDomainFuncDeclNode* node, clang::SourceManager& mgr)
{
	int id = 0;
	auto funcBody = mapToRdfNode(node->getBody(), id, mgr, true);
	return new ControlFlowDomainAlgorythmRdfNode("testAlgo", ++id, (ControlFlowDomainSequenceRdfNode*)funcBody);
}

