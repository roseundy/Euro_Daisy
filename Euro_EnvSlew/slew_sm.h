//
// Slew State Machine

#ifndef SLEW_SM_H_
#define SLEW_SM_H_

class SlewSM
{
  public:
    SlewSM() {}
    ~SlewSM() {}

    /** 
    Initializes the state machine
    */
    void Init()
    {
        attack_phase_ = false;
        sustain_phase_ = false;
        triggered_ = false;
        trigger_last_ = false;
        gated_ = false;
        loop_armed_ = false;
        loop_last_ = false;
        reset_out_ = false;
    }

    /** 
    This processes the state machine
    */
    void Process(float trigger, bool gate, bool loop, bool rising, bool falling)
    {
        if (trigger && !triggered_ && !trigger_last_) {
            triggered_ = true;
            gated_ = false;
            reset_out_ = true;
            attack_phase_ = true;
            sustain_phase_ = false;
            trigger_last_ = true;
            return;
        }
        trigger_last_ = trigger;

        if (gate && !gated_) {
            gated_ = true;
            triggered_ = false;
            reset_out_ = true;
            attack_phase_ = true;
            sustain_phase_ = false;
            return;
        }

        if (triggered_) {
            if (reset_out_) {
                reset_out_ = false;
                return;
            } else if (!rising) {
                attack_phase_ = false;
                triggered_ = false;
                falling = true;
            }
        }

        if (gated_) {
            if (reset_out_) {
                reset_out_ = false;
                return;
            } else if (!gate) {
                attack_phase_ = false;
                sustain_phase_ = false;
                gated_ = false;
            } else if (attack_phase_ && !rising) {
                attack_phase_ = false;
                sustain_phase_ = true;
                falling = true;
            } 
        }

        if (loop && !triggered_ && !gated_ && (falling || !loop_last_)) {
            loop_armed_ = true;
        } else if (loop_armed_ && !triggered_ && !gated_ && !falling) {
            loop_armed_ = false;
            triggered_ = true;
            gated_ = false;
            reset_out_ = true;
            attack_phase_ = true;
            sustain_phase_ = false;
        }
        loop_last_ = loop;
    }

    /** Return if in attack phase
    */
    inline bool attackPhase() { return attack_phase_; }

    /** Return if in sustain phase
    */
    inline bool sustainPhase() { return sustain_phase_; }

    /** Return if resetting
    */
    inline bool resetOut() { return reset_out_; }

  private:
    bool    attack_phase_;
    bool    sustain_phase_;
    bool    reset_out_;
    bool    triggered_;
    bool    trigger_last_;
    bool    gated_;
    bool    loop_armed_;
    bool    loop_last_;
};

#endif // SLEW_H_