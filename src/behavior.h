// -----------------------------------------------------------------------------
//
// Copyright (C) Lukas Breitwieser.
// All Rights Reserved.
//
// -----------------------------------------------------------------------------

#ifndef BEHAVIOR_H_
#define BEHAVIOR_H_

#include "core/behavior/behavior.h"

#include "person.h"
#include "sim-param.h"

namespace bdm {

// -----------------------------------------------------------------------------
struct CheckSurrounding : public Functor<void, Agent*, double> {
  Person* self_;

  CheckSurrounding(Person* self) : self_(self) {}

  // This function operator will be called for every other person within
  // `infection_radius`
  void operator()(Agent* neighbor, double squared_distance) override {
    auto* other = bdm_static_cast<const Person*>(neighbor);
    if (other->state_ == State::kInfected || other->state_ == State::kNoSymptoms) {
      self_->state_ = State::kInfected;
    }
  }
};

// -----------------------------------------------------------------------------
struct Die : public Behavior {
  BDM_BEHAVIOR_HEADER(Die, Behavior, 1);

  Die() {}

  void Run(Agent* a) override {
    auto* sim = Simulation::GetActive();
    auto* random = sim->GetRandom();
    auto* param = sim->GetParam();
    auto* sparam = param->Get<SimParam>();

    auto* person = bdm_static_cast<Person*>(a);
    if (person->state_ == kIsolated &&
        random->Uniform(0, 1) <= sparam->death_probability) {
      auto f = (person->GetAllBehaviors()).make_std_vector();
      for (auto x: f){
        person->RemoveBehavior(x);
      }
      person->state_ = kDead;
      Person::dead++;
    }
  }
};

// -----------------------------------------------------------------------------
struct Infection : public Behavior {
  BDM_BEHAVIOR_HEADER(Infection, Behavior, 1);

  Infection() {}

  void Run(Agent* a) override {
    auto* sim = Simulation::GetActive();
    auto* random = sim->GetRandom();
    auto* param = sim->GetParam();
    auto* sparam = param->Get<SimParam>();

    auto* person = bdm_static_cast<Person*>(a);
    if (person->state_ == kSusceptible &&
        random->Uniform(0, 1) <= sparam->infection_probablity) {
      auto* ctxt = sim->GetExecutionContext();
      CheckSurrounding check(person);
      ctxt->ForEachNeighbor(check, *person, sparam->infection_radius);
      //----------------------------------
      person->AddBehavior(new Die());
      person->state_ = State::kInfected;
      Person::infected++;
      Person::susceptible--;
    }
  }
};

// -----------------------------------------------------------------------------
struct Recovery : public Behavior {
  BDM_BEHAVIOR_HEADER(Recovery, Behavior, 1);

  Recovery() {}

  void Run(Agent* a) override {
    auto* person = bdm_static_cast<Person*>(a);
    if (person->state_ == kNoSymptoms || person->state_ == kIsolated) {
      auto* sim = Simulation::GetActive();
      auto* random = sim->GetRandom();
      auto* sparam = sim->GetParam()->Get<SimParam>();
      if (random->Uniform(0, 1) <= sparam->recovery_probability) {
        // Our modification: delete method Die() from recovered people
        //-----------------------------------------------------------------
        auto f = (person->GetAllBehaviors()).make_std_vector();
        const std::type_info& ti1 = typeid(Die);
      for (auto x: f){
        const std::type_info& ti2 = typeid(x);

          if(ti1.hash_code() == ti2.hash_code()){ // guaranteed
            person->RemoveBehavior(x);
            break;
          }
      }
      //------------------------------------------------------------------
        person->state_ = State::kRecovered;
        Person::recovered++;
      }
    }
  }
};

// -----------------------------------------------------------------------------
struct RandomMovement : public Behavior {
  BDM_BEHAVIOR_HEADER(RandomMovement, Behavior, 1);

  RandomMovement() {}

  void Run(Agent* agent) override {
    auto* sim = Simulation::GetActive();
    auto* random = sim->GetRandom();
    auto* param = sim->GetParam();
    auto* sparam = param->Get<SimParam>();

    const auto& position = agent->GetPosition();
    auto rand_movement = random->UniformArray<3>(-1, 1).Normalize();
    rand_movement.back() = 0;
    auto new_pos = position + rand_movement * sparam->agent_speed;
    for (auto& el : new_pos) {
      el = std::fmod(el, param->max_bound);
      el = el < 0 ? param->max_bound + el : el;
    }

    //-----------------------------------------
    auto* person = bdm_static_cast<Person*>(agent);

    switch(person->state_){

    case kSusceptible:
      if (random->Uniform(0, 1) <= sparam->vaccination_probability) {
        person->state_ = State::kVaccinated;
        Person::vaccinated++;
      }
      break;

    case kInfected:

      if (person->incubation < 0){
        if(random->Uniform(0, 1) <= sparam->end_of_incubation_probability) {
          person->incubation = 1e9;
        }
      }
      else if (person->incubation < sparam->incubation_period){
        person->incubation++;
      }
      else if (person->incubation == sparam->incubation_period){
                person->incubation = -1;
      }
      else{

        if (random->Uniform(0, 1) <= sparam->no_symptoms_probability){
          person->state_ = State::kNoSymptoms;
        }
        else{
          person->state_ = State::kIsolated;
          Person::isolated++;
        }

      }
      break;


      default:
        break;

    }


    
    //-----------------------------------------

    if(person->state_ != State::kIsolated){
      agent->SetPosition(new_pos);
    }
  }
};

}  // namespace bdm

#endif  // BEHAVIOR_H_
