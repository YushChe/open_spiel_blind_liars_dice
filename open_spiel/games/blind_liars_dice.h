//
// Created by dog on 15/08/2022.
//

#ifndef OPEN_SPIEL_BLIND_LIARS_DICE_BLIND_LIARS_DICE_H
#define OPEN_SPIEL_BLIND_LIARS_DICE_BLIND_LIARS_DICE_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

// A simple game that includes chance and imperfect information
// https://en.wikipedia.org/wiki/Liar%27s_dice
//
// Currently only supports a single round and two players.
//
// Parameters:
//   "bidding_rule" string   bidding variants ("reset-face" or
//                           ("reset-quantity")              (def. "reset-face")
//   "dice_sides"   int      number of sides on each die            (def. = 6)
//   "numdice"      int      number of dice per player              (def. = 1)
//   "numdiceX"     int      overridden number of dice for player X (def. = 1)
//   "players"      int      number of players                      (def. = 2)

namespace open_spiel {
    namespace blind_liars_dice {

        enum BiddingRule {
            // The player may bid a higher quantity of any particular face, or the same
            // quantity of a higher face (allowing a player to "re-assert" a face value
            // they believe prevalent if another player increased the face value on their
            // bid).
            kResetFace = 1,

            // The player may bid a higher quantity of the same face, or any particular
            // quantity of a higher face (allowing a player to "reset" the quantity).
            kResetQuantity = 2
        };

        class BlindLiarsDiceGame;

        class BlindLiarsDiceState : public State {
        public:
            explicit BlindLiarsDiceState(std::shared_ptr<const Game> game, int total_num_dice,
                                         int max_dice_per_player,
                                         const std::vector<int>& num_dice);
            BlindLiarsDiceState(const BlindLiarsDiceState&) = default;

            void Reset(const GameParameters& params);
            Player CurrentPlayer() const override;
            std::string ActionToString(Player player, Action action_id) const override;
            std::string ToString() const override;
            bool IsTerminal() const override;
            std::vector<double> Returns() const override;
            std::string InformationStateString(Player player) const override;
            void InformationStateTensor(Player player,
                                        absl::Span<float> values) const override;
            void ObservationTensor(Player player,
                                   absl::Span<float> values) const override;
            std::unique_ptr<State> Clone() const override;
            std::vector<std::pair<Action, double>> ChanceOutcomes() const override;
            std::vector<Action> LegalActions() const override;

            // Return number of sides on the dice.
            const int dice_sides() const;

            Player calling_player() const { return calling_player_; }
            int dice_outcome(Player player, int index) const {
                return dice_outcomes_[player][index];
            }
            int last_bid() const {
                if (bidseq_.back() == total_num_dice_ * dice_sides()) {
                    return bidseq_[bidseq_.size() - 2];
                } else {
                    return bidseq_.back();
                }
            }

        protected:
            void DoApplyAction(Action action_id) override;

            // Get the quantity and face of the bid from an integer. The format of the
            // return depends on the bidding rule.
            // The bids starts at 0 and go to total_dice*dice_sides-1 (inclusive).
            std::pair<int, int> UnrankBid(int bid) const;

            // Dice outcomes: first indexed by player, then by dice number
            std::vector<std::vector<int>> dice_outcomes_;
            std::vector<std::vector<int>> blind_dice_outcomes_;

            // The bid sequence.
            std::vector<int> bidseq_;

        private:
            void ResolveWinner();

            // Return the bidding rule used by the game.
            const BiddingRule bidding_rule() const;

            // Initialized to invalid values. Use Game::NewInitialState().
            Player cur_player_;  // Player whose turn it is.
            int cur_roller_;     // Player currently rolling dice.
            int winner_;
            int loser_;
            int current_bid_;
            int total_num_dice_;
            int total_moves_;
            int calling_player_;  // Player who calls Liar.
            int bidding_player_;  // Player who cast the last bid.
            int max_dice_per_player_;

            std::vector<int> num_dice_;         // How many dice each player has.
            std::vector<int> num_dice_rolled_;  // Number of dice currently rolled.

            // Used to encode the information state.
            std::string bidseq_str_;
        };

        class BlindLiarsDiceGame : public Game {
        public:
            explicit BlindLiarsDiceGame(const GameParameters& params, GameType game_type);
            int NumDistinctActions() const override;
            std::unique_ptr<State> NewInitialState() const override;
            int MaxChanceOutcomes() const override;
            int NumPlayers() const override { return num_players_; }
            double MinUtility() const override { return -1; }
            double MaxUtility() const override { return 1; }
            double UtilitySum() const override { return 0; }
            std::vector<int> InformationStateTensorShape() const override;
            std::vector<int> ObservationTensorShape() const override;
            int MaxGameLength() const override;
            int MaxChanceNodesInHistory() const override;

            // Returns the maximum among how many dice each player has. For example,
            // if player 1 has 3 dice and player 2 has 2 dice, returns 3.
            int max_dice_per_player() const { return max_dice_per_player_; }

            // Return the total number of dice in the game.
            int total_num_dice() const { return total_num_dice_; }

            // Return the number of dice each player has.
            std::vector<int> num_dice() const { return num_dice_; }

            int blind_player_id() const { return blind_player_id_; }
            const int dice_sides() const { return dice_sides_; }
            const BiddingRule bidding_rule() const { return bidding_rule_; }

        private:
            // Number of players.
            int num_players_;

            // Total dice in the game, determines the legal bids.
            int total_num_dice_;
            int blind_player_id_;
            std::vector<int> num_dice_;  // How many dice each player has.
            int max_dice_per_player_;    // Maximum value in num_dice_ vector.
            const int dice_sides_;       // Number of faces on each die.
            const BiddingRule bidding_rule_;
        };

// Implements the action abstraction from Lanctot et al. '12
// http://mlanctot.info/files/papers/12icml-ir.pdf. See also Neller & Hnath,
// Approximating Optimal Dudo Play with Fixed-Strategy Iteration Counterfactual
// Regret Minimization: https://core.ac.uk/download/pdf/205864381.pdf
//
// This game has an extra parameter:
//   "recall_length"    int      number of bids to remember     (def. = 4)

        class ImperfectRecallLiarsDiceState : public BlindLiarsDiceState {
        public:
            ImperfectRecallLiarsDiceState(std::shared_ptr<const Game> game,
                                          int total_num_dice,
                                          int max_dice_per_player,
                                          const std::vector<int>& num_dice)
                    : BlindLiarsDiceState(game, total_num_dice, max_dice_per_player, num_dice) {}
            std::string InformationStateString(Player player) const override;
            std::unique_ptr<State> Clone() const override {
                return std::unique_ptr<State>(new ImperfectRecallLiarsDiceState(*this));
            }
        };

        class ImperfectRecallBlindLiarsDiceGame : public BlindLiarsDiceGame {
        public:
            explicit ImperfectRecallBlindLiarsDiceGame(const GameParameters& params);
            std::unique_ptr<State> NewInitialState() const override;

            int recall_length() const { return recall_length_; }

        private:
            int recall_length_;
        };


    }  // namespace blind_liars_dice
}  // namespace open_spiel

#endif //OPEN_SPIEL_BLIND_LIARS_DICE_BLIND_LIARS_DICE_H
