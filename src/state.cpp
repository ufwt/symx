#include"context.h"
#include"expr.h"
#include"state.h"

Block state_create_block(Context *ctx) {
	unsigned int i;
	Block blk =  Block(ctx->num_reg,ctx->num_flag);

	blk.mem = BytMem::create_dangle(0);
	for(i = 0; i < blk.num_reg; i++) {
		blk.reg[i] = BytVec::create_dangle(ctx->reg_size,i);
	}
	for(i = 0;i < blk.num_flag; i++) {
		blk.flag[i] = Cond::create_false();
	}
	return blk;
}
