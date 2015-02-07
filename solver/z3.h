#include<z3.h>
#include<memory>
#include<unordered_map>
#include"utils.h"
#include"context.h"
#include"expr.h"
#include"state.h"

#ifndef _SOLVER_H_
#define _SOLVER_H_

#ifndef LOG_PREFIX
#define LOG_PREFIX "z3_solver"
#endif

namespace z3_solver {
	class Z3Solver;
	class Z3SolvExpr;
	class Z3SolvCond;
	typedef std::shared_ptr<Z3SolvExpr> refZ3SolvExpr;
	typedef std::shared_ptr<Z3SolvCond> refZ3SolvCond;

	class Z3TransVisitor : public symx::TransVisitor {
		public:
			Z3TransVisitor(
				const Z3Solver *_solver,
				const symx::refSolvExpr mem,
				const std::unordered_map
					<unsigned int,symx::refSolvExpr> &reg,
				const std::unordered_map
					<unsigned int,symx::refSolvCond> &flag
			);
			~Z3TransVisitor();
			symx::refSolvExpr get_solver_expr(
					const symx::refExpr expr);
			symx::refSolvCond get_solver_cond(
					const symx::refCond cond);
			int pre_visit(symx::refBytVec vec);
			int pre_visit(symx::refBytMem mem);
			int pre_visit(symx::refOperator oper);
			int pre_visit(symx::refCond cond);
			int visit(symx::refBytVec vec);
			int visit(symx::refBytMem mem);
			int visit(symx::refOperator oper);
			int visit(symx::refCond cond);
		private:
			const Z3Solver *solver;
			const symx::refSolvExpr dangle_mem;
			const std::unordered_map
				<unsigned int,symx::refSolvExpr> &dangle_reg;
			const std::unordered_map
				<unsigned int,symx::refSolvCond> &dangle_flag;
			Z3_sort bvsort1;
			Z3_sort bvsort4;
			Z3_ast bvimm41;
			std::unordered_map<symx::refExpr,Z3_ast> expr_ast;
			std::unordered_map<symx::refCond,Z3_ast> cond_ast;
			Z3_ast expr_to_ast(const symx::refExpr expr);
			Z3_ast cond_to_ast(const symx::refCond cond);
	};
	class Z3Solver : public symx::Solver {
		public:
			Z3_context context;
			Z3_solver solver;

			Z3Solver();
			symx::TransVisitor* create_translator();
			symx::TransVisitor* create_translator(
				const symx::refSolvExpr mem,
				const std::unordered_map
					<unsigned int,symx::refSolvExpr> &reg,
				const std::unordered_map
					<unsigned int,symx::refSolvCond> &flag
			);
			bool solve(
				const std::vector<symx::refSolvCond> &cons,
				std::unordered_map
					<symx::refSolvExpr,uint64_t> *var
			);

		private:
			static void error_handler(
				Z3_context ctx,
				Z3_error_code error
			) {
				err("Z3 Solver: %s\n",
					Z3_get_error_msg_ex(ctx,error));
			}
	};
	class Z3SolvExpr : public symx::SolvExpr {
		public:
			Z3_context context;
			Z3_ast ast;
			Z3SolvExpr(Z3_context _context,Z3_ast _ast);
			~Z3SolvExpr();
	};
	class Z3SolvCond : public symx::SolvCond {
		public:
			Z3_context context;
			Z3_ast ast;
			Z3SolvCond(Z3_context _context,Z3_ast _ast);
			~Z3SolvCond();
	};
};

#endif
