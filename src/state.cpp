#define LOG_PREFIX "state"

#include<memory>
#include<string>
#include<unordered_map>
#include<vector>

#include"utils.h"
#include"context.h"
#include"expr.h"
#include"state.h"
#include"solver.h"

using namespace symx;

namespace symx {

class PrintVisitor : public ExprVisitor {
	public:
		int print(refExpr expr){
			info("%s\n",expr_map[expr].c_str());
			return 0;
		}
		int visit(refBytMem mem) {
			if(mem->type == ExprMem) {
				expr_map[mem] = "(mem " +
					std::to_string(mem->id) +
					")";
			} else if(mem->type == ExprDangle) {
				expr_map[mem] = "(dmem)";
			}
			return 0;
		}
		int visit(refBytVec vec) {
			if(vec->type == ExprImm) {
				expr_map[vec] = "(imm " +
					std::to_string(vec->data) +
					")";
			} else if(vec->type == ExprVar) {
				expr_map[vec] = "(var " +
					std::to_string(vec->id) +
					")";
			} else if(vec->type == ExprDangle) {
				expr_map[vec] = "(dvec " +
					std::to_string(vec->index) +
					")";
			}
			return 0;
		}
		int visit(refOperator oper) {
			unsigned int i;
			std::string params;

			switch(oper->type) {
			case ExprOpSelect:
				params = "select " +
					expr_map[oper->operand[0]] + "," +
					expr_map[oper->operand[1]];
				break;
			case ExprOpExtract:
				params = "extract " +
					expr_map[oper->operand[0]] + "," +
					std::to_string(oper->start);
				break;
			default:
				params = expr_map[oper->operand[0]];
				for(i = 1; i < oper->op_count; i++) {
					params += "," +
						expr_map[oper->operand[i]];
				}
				break;
			}
			expr_map[oper] = "(" + params + ")";
			return 0;
		}
		int visit(refCond cond) {
			return 0;
		}
	private:
		std::unordered_map<refExpr,std::string> expr_map;
};
refExpr BuildVisitor::get_expr(const refExpr expr) {
	auto it = expr_map.find(expr);
	if(it == expr_map.end()) {
		err("expr not exist\n");
		return nullptr;
	}
	return it->second;
}
refCond BuildVisitor::get_cond(const refCond cond) {
	auto it = cond_map.find(cond);
	if(it == cond_map.end()) {
		err("cond not exist\n");
		return nullptr;
	}
	return it->second;
}
int BuildVisitor::pre_visit(symx::refBytVec vec) {
	if(expr_map.find(vec) != expr_map.end()) {
		return 0;
	}
	return 1;
}
int BuildVisitor::pre_visit(symx::refBytMem mem) {
	if(expr_map.find(mem) != expr_map.end()) {
		return 0;
	}
	return 1;
}
int BuildVisitor::pre_visit(symx::refOperator oper) {
	if(expr_map.find(oper) != expr_map.end()) {
		return 0;
	}
	return 1;
}
int BuildVisitor::pre_visit(symx::refCond cond) {
	if(cond_map.find(cond) != cond_map.end()) {
		return 0;
	}
	return 1;
}
int BuildVisitor::visit(symx::refBytVec vec) {
	if(vec->type == ExprDangle) {
		expr_map[vec] = state->reg[vec->index];
	} else {
		expr_map[vec] = ref<BytVec>(vec);
	}
	return 1;
}
int BuildVisitor::visit(symx::refBytMem mem) {
	if(mem->type == ExprDangle) {
		expr_map[mem] = state->mem;
	} else {
		expr_map[mem] = ref<BytMem>(mem);
	}
	return 1;
}
int BuildVisitor::visit(symx::refOperator oper) {
	switch(oper->type) {
	case ExprOpSelect:
		expr_map[oper] = expr_select(
			expr_map[oper->operand[0]],
			expr_map[oper->operand[1]],
			oper->size);
		break;
	case ExprOpExtract:
		expr_map[oper] = expr_extract(
			expr_map[oper->operand[0]],
			oper->start,
			oper->start + oper->size);
		break;
	default:
		if(oper->op_count == 1) {
			expr_map[oper] = ref<Operator>(
				oper->type,
				oper->size,
				expr_map[oper->operand[0]]);
		} else if(oper->op_count == 2) {
			expr_map[oper] = ref<Operator>(
				oper->type,
				oper->size,
				expr_map[oper->operand[0]],
				expr_map[oper->operand[1]]);
		} else if(oper->op_count == 3) {
			expr_map[oper] = ref<Operator>(
				expr_map[oper->operand[0]],
				expr_map[oper->operand[1]],
				expr_map[oper->operand[2]]);
		}
		break;
	}
	return 1;
}
int BuildVisitor::visit(symx::refCond cond) {
	switch(cond->type) {
	case CondDangle:
		cond_map[cond] = Cond::create_dangle(cond->index);
		break;
	default:
		if(cond->cond_count == 0 && cond->expr_count == 2) {
			cond_map[cond] = ref<Cond>(
				cond->type,
				expr_map[cond->expr[0]],
				expr_map[cond->expr[1]]);
		} else if(cond->cond_count == 1 && cond->expr_count == 0) {
			cond_map[cond] = ref<Cond>(
				cond->type,
				cond_map[cond->cond[0]]);

		} else if(cond->cond_count == 2 && cond->expr_count == 0) {
			cond_map[cond] = ref<Cond>(
				cond->type,
				cond_map[cond->cond[0]],
				cond_map[cond->cond[1]]);

		}
		break;
	}
	return 1;
}

refBlock state_create_block(Context *ctx) {
	unsigned int i;
	refBlock blk =  ref<Block>();

	blk->mem = BytMem::create_dangle(-1);
	for(i = 0; i < ctx->num_reg; i++) {
		blk->reg[i] = BytVec::create_dangle(ctx->reg_size,i);
	}
	for(i = 0;i < ctx->num_flag; i++) {
		blk->flag[i] = Cond::create_dangle(i);
	}
	return blk;
}
static refState create_static_state(Context *ctx,refProbe probe,uint64_t pc) {
	unsigned int i;
	auto nstate = ref<State>(pc,probe);
	auto vis = ctx->solver->create_translator();

	nstate->mem = BytMem::create_var(ctx);
	expr_walk(vis,nstate->mem);
	for(i = 0; i < ctx->num_reg; i++) {
		nstate->reg[i] = BytVec::create_imm(
			ctx->reg_size,
			probe->read_reg(i));
		expr_walk(vis,nstate->reg[i]);
	}
	for(i = 0; i < ctx->num_flag; i++) {
		if(probe->read_flag(i)) {
			nstate->flag[i] = Cond::create_true();
		} else {
			nstate->flag[i] = Cond::create_false();
		}
		expr_walk(vis,nstate->flag[i]);
	}

	delete vis;
	return nstate;
}
int state_executor(Context *ctx,refProbe probe,uint64_t pc) {
	unsigned int i;
	Solver *solver = ctx->solver;
	refState nstate,cstate;
	refBlock cblk;
	std::unordered_map<unsigned int,refSolvExpr> solver_reg;
	std::unordered_map<unsigned int,refSolvCond> solver_flag;
	std::vector<refSolvCond> cons;
	std::unordered_map<refSolvExpr,uint64_t> var;

	nstate = create_static_state(ctx,probe,pc);
	ctx->state.push(nstate);

	while(!ctx->state.empty()) {
		cstate = ctx->state.front();
		ctx->state.pop();

		auto blk_it = ctx->block.find(pc);
		if(blk_it == ctx->block.end()) {
			cblk = ctx->interpret(probe,pc);
		} else {
			cblk = blk_it->second;
		}

		nstate = ref<State>(cstate->pc,cstate->probe);

		auto build_vis = new BuildVisitor(cstate);
		expr_walk(build_vis,cblk->mem);
		nstate->mem = build_vis->get_expr(cblk->mem);
		for(i = 0; i < ctx->num_reg; i++) {
			expr_walk(build_vis,cblk->reg[i]);
			nstate->reg[i] = build_vis->get_expr(cblk->reg[i]);
		}
		for(i = 0; i < ctx->num_flag; i++) {
			expr_walk(build_vis,cblk->flag[i]);
			nstate->flag[i] = build_vis->get_cond(cblk->flag[i]);
		}
		delete build_vis;

		/*
		auto vis = new PrintVisitor();
		expr_walk(vis,cblk->mem);
		vis->print(cblk->mem);
		delete vis;
		for(i = 0; i < ctx->num_reg; i++) {
			auto vis = new PrintVisitor();
			expr_walk(vis,cblk->reg[i]);
			vis->print(cblk->reg[i]);
			delete vis;
		}
		*/

		auto trans_vis = solver->create_translator();
		expr_walk(trans_vis,cblk->mem);
		for(i = 0;i < ctx->num_reg;i++) {
			expr_walk(trans_vis,cblk->reg[i]);
		}
		for(i = 0;i < ctx->num_flag;i++) {
			expr_walk(trans_vis,cblk->flag[i]);
		}

		auto solexpr_pc = cblk->reg[ctx->REGIDX_PC]->solver_expr;
		cons.clear();
		var.clear();
		var[solexpr_pc] = 0;
		while(true) {
			if(!solver->solve(cons,&var)) {
				break;	
			}

			uint64_t next_pc = var[solexpr_pc];
			info("next pc %lx\n",next_pc);

			auto exclude_cond = cond_not(
				cond_eq(
					cblk->reg[ctx->REGIDX_PC],
					BytVec::create_imm(4,next_pc)));
			expr_walk(trans_vis,exclude_cond);
			cons.push_back(exclude_cond->solver_cond);
		}

		delete trans_vis;
		break;
	}
	return 0;
}

};
