#ifndef SAMPLE_PLAYER_H_
#define SAMPLE_PLAYER_H_

class SamplePlayer
{
  public:
    SamplePlayer() {}
    ~SamplePlayer() {}

    void Init(volatile int *up_pnt) {
        buf_ = NULL;
        ptr_ = NULL;
        cnt_ = 0;
        size_ = 0;
        hash_ = 0xffffffff;
        running_ = false;
        updating_ = up_pnt;
    }

    float Process(bool trigger) {
        if (*updating_) {
            running_ = false;
            return 0.f;
        }

        if (!running_ && !trigger)
            return 0.f;

        float samp;

        if (trigger) {
            ptr_ = buf_;
            cnt_ = 0;
            running_ = true;
        }

        samp = s162f(*ptr_);
        cnt_ += 2;
        if (cnt_ >= size_)
            running_ = false;
        else
            ptr_++;

        return samp;
    }

    void SetSample(int dno, int sno, int16_t *buf, unsigned int size) {
        if (*updating_)
            return;

        unsigned int new_hash = (dno << 16) | sno;
        if (new_hash != hash_) {
            hash_ = new_hash;
            buf_ = buf;
            size_ = size;
            cnt_ = 0;
            ptr_ = buf_;
        }
    }

    bool Playing() {
        return running_;
    }

    private:
        volatile bool running_;
        int16_t *buf_;
        int16_t *ptr_;
        unsigned int cnt_;
        unsigned int size_;
        unsigned int hash_;
        volatile int *updating_;
};


#endif // SAMPLE_PLAYER_H