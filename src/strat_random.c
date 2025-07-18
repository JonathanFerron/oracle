/*
 * Order of events in the 'play_turn' function:
 * 
 * attackPhase() 
 * defensePhase()
 * resolveCombat(), which will swith to turn_phase = attachk as the last line of code
 * 
 * attack_phase():
 *   apply_attack_strat(curPlayer) = apply_attack_def_strat[curPlayer, Attack (enum)]
 *      which will switch to turn_phase = defense as the last line of code
 * 
 * defense_phase():
 *   if combatZone[curPlayer] > 0
 *     apply_defense_strat[notCurPlayer] =  apply_attack_def_strat[notCurPlayer, Defense (enum)]
 *   endif
 * 
 * AttStratA, etc are 4 arrays of function pointers to be executed sequentially to execute the Attack Strategy for Player A, player B, etc.
 * void (*AttStratA[n]) (&gamestat, &gamestate, ...): n is the maximum number of steps in a strategy
 * void (*AttStratB[n]) (&gamestat, &gamestate, ...)
 * void (*DefStratA[n]) (&gamestat, &gamestate, ...)
 * void (*DefStratB[n]) (&gamestat, &gamestate, ...)
 * 
 * AttStratA[0] = function name, etc.; or initialize this way: AttStratA = {f1, f2, ...}
 * 
 * ApplyAttDefStrat(PlayerID, AttackDefenseIndicator (from an enum))
 *  function that will find, based on PlayerID and A/D ID which of the 4 array of function pointers to use, and will then call them
 * 
 * inside ApplyAttDefStrat, 
 *   define void (*Strat[n]) (&gamestat, &gamestate, ...);
 * then based on PlayerID and A/D ID in switch case stmts, could then do:
 *   Strat = AttStratA;
 * 
 * then loop over n indices of Strat and call functions:
 *   for each i from 0 to n - 1
 *     Strat[i] (&gamestat, &gamestate, ...);
 */
