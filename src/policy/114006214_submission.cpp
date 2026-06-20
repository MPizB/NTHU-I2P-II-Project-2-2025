#include <utility>
#include "114006214_state.hpp"
#include "114006214_submission.hpp"
#include <algorithm>


/*============================================================
 * MiniMax — eval_ctx
 *
 * Negamax without pruning. Caller manages memory.
 *============================================================*/
struct ScoredMove {
    Move move;
    int score;
};

int MiniMax::quiescence(
    State *state,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){ ctx.seldepth = ply; }
    if(ctx.stop){ return 0; }

    //Standing Pat baseline
    int standing_pat = state->evaluate(p.use_kp_eval, p.use_eval_mobility, &history);
    
    if (standing_pat >= beta) {
        return standing_pat;
    }
    if (standing_pat > alpha) {
        alpha = standing_pat;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    // return the score for a winning terminal state
    if(state->game_state == WIN) {
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    std::vector<ScoredMove> captures;
    int us = state->player;
    int them = 1 - us;

    for (const auto& action : state->legal_actions) {
        int victim = state->piece_at(them, action.second.first, action.second.second);
        if (victim > 0) { // Only capture moves
            int attacker = state->piece_at(us, action.first.first, action.first.second);
            int score = 10000 + (PIECE_VALUES[victim] * 10) - PIECE_VALUES[attacker];
            captures.push_back({action, score});
        }
    }

    //Sort captures with MVV-LVA
    std::sort(captures.begin(), captures.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    /* Negamax loop */
    for (const auto& sm : captures) {
        State* next = state->next_state(sm.move);
        bool same = next->same_player_as_parent();
        int score;

        if (same) {
            score = quiescence(next, alpha, beta, history, ply + 1, ctx, p);
        } else {
            score = -quiescence(next, -beta, -alpha, history, ply + 1, ctx, p);
        }

        delete next;

        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            return alpha; // Beta Cutoff
        }
    }

    return alpha;
}


 int MiniMax::eval_ctx(
    State *state,
    int depth,
    int alpha, // alpha-beta pruning
    int beta, // alpha-beta pruning
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    /* === Lazy move generation (sets game_state) === */
    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    /* === Terminal / leaf checks === */

    // [ Hackathon TODO 3-1 ]
    // return the score for a winning terminal state
    // Hint: prefer faster wins by using ply.
    if(state->game_state == WIN) {
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    /* === Repetition check (game-specific) === */
    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    history.push(state->hash());

    if(depth <= 0){
        //extend lookahead phase when reaching max depth
        int score = quiescence(state, alpha, beta, history, ply, ctx, p);
        history.pop(state->hash());
        return score;
    }

    //Move ordering (MVV - LVA)
    std::vector<ScoredMove> scored_moves;
    scored_moves.reserve(state->legal_actions.size());

    int us = state->player;
    int them = 1 - us;

    for (const auto& action : state->legal_actions) {
        int score = 0;
        Point from = action.first;
        Point to = action.second;

        // Identify attacker & victim
        int attacker = state->piece_at(us, from.first, from.second);
        int victim = state->piece_at(them, to.first, to.second);

        if (victim > 0) {
            // MVV-LVA
            score = 10000 + (PIECE_VALUES[victim] * 10) - PIECE_VALUES[attacker];
        }

        scored_moves.push_back({action, score});
    }

    std::sort(scored_moves.begin(), scored_moves.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    /* === Negamax loop === */
    int best_score = M_MAX;
    bool is_first = true;

    for(const auto& sm : scored_moves){
        // [ Hackathon TODO 3-2 ]
        // create the child state after applying action
        Move action = sm.move;
        State* next = state->next_state(action);

        bool same = next->same_player_as_parent();

        // [Hackathon TODO 3-3]
        // search the child one level deeper
        // [Hackathon TODO 3-4]
        // convert raw to the current player's perspective.
        int score;
        if (same) {
            if(is_first) {
                //return maximum value
                score = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
            }
            else {
                score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, ply + 1, ctx, p);
                if (score > alpha && score < beta) {
                    score = eval_ctx(next, depth - 1, score, beta, history, ply + 1, ctx, p);
                }
            }
        }
        else {
            if(is_first) {
                //return minimum value
                score = -eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
            }
            else {
                // Search subsequent moves with null window
                score = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p);
                
                // If the move breaks our assumption (fail high), re-search it
                if (score > alpha && score < beta) {
                    score = -eval_ctx(next, depth - 1, -beta, -score, history, ply + 1, ctx, p);
                }
            }
        }
        

        delete next;

        // [ Hackathon TODO 3-5 ]
        // update best_score if this child is better.
        if (score > alpha) {
            alpha = score;
        }
        
        if (alpha >= beta) {
            history.pop(state->hash());
            return alpha;
        }
        is_first = false;
    }

    
}


/*============================================================
 * MiniMax — search
 *
 * Iterate legal moves, call eval_ctx, return SearchResult.
 *============================================================*/
SearchResult MiniMax::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    int alpha = M_MAX; //(-100000)
    int beta = P_MAX; //(+100000)

    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    //Move ordering (MVV - LVA)
    std::vector<ScoredMove> scored_moves;
    scored_moves.reserve(total_moves);

    int us = state->player;
    int them = 1 - us;

    for (const auto& action : state->legal_actions) {
        int score = 0;
        int attacker = state->piece_at(us, action.first.first, action.first.second);
        int victim = state->piece_at(them, action.second.first, action.second.second);

        if (victim > 0) {
            score = 10000 + (PIECE_VALUES[victim] * 10) - PIECE_VALUES[attacker];
        }
        scored_moves.push_back({action, score});
    }

    std::sort(scored_moves.begin(), scored_moves.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });


    bool is_first = true;
    for(const auto& sm : scored_moves){
        /* [ Hackathon TODO 4-1 ]
         * search this move like TODO 3, but starting from the root */
        Move action = sm.move;
        
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int score;

        if (same) {
            if (is_first) {
                //return maximum value
                score = eval_ctx(next, depth - 1, alpha, beta, history, 1, ctx, p);
            } else {
                score = eval_ctx(next, depth - 1, alpha, alpha + 1, history, 1, ctx, p);
                if (score > alpha && score < beta) {
                    score = eval_ctx(next, depth - 1, score, beta, history, 1, ctx, p);
                }
            }
        } else {
            if (is_first) {
                //return minimum value
                score = -eval_ctx(next, depth - 1, -beta, -alpha, history, 1, ctx, p);
            } else {
                score = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, 1, ctx, p);
                if (score > alpha && score < beta) {
                    score = -eval_ctx(next, depth - 1, -beta, -score, history, 1, ctx, p);
                }
            }
        }
        delete next;

            if(score > alpha){
                // [ Hackathon TODO 4-2 ]
                // keep this move if it is the best so far
                alpha = score;
                result.best_move = action;
                
                if(p.report_partial && ctx.on_root_update){
                   ctx.on_root_update({result.best_move, alpha, depth, move_index + 1, total_moves});
                }
            }  
        move_index++;
        is_first = false;
    }

    // [ Hackathon TODO 4-3 ]
    // update result and return
    result.score = alpha;
    result.nodes = ctx.nodes;
    result.depth = depth;
    result.seldepth = ctx.seldepth;
    result.pv = {result.best_move};
    return result;
} 


/*============================================================
 * MiniMax — default_params / param_defs
 *============================================================*/
ParamMap MiniMax::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> MiniMax::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
