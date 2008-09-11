/* The Channel class, provides a simple, Ruby thread safe communication
 * mechanism. */

#include "vm.hpp"
#include "objectmemory.hpp"

#include "builtin/channel.hpp"
#include "builtin/thread.hpp"
#include "builtin/list.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/float.hpp"
#include "builtin/io.hpp"

#include "event.hpp"

namespace rubinius {
  Channel* Channel::create(STATE) {
    Channel* chan = (Channel*)state->new_object(G(channel));
    SET(chan, waiting, List::create(state));

    return chan;
  }

  OBJECT Channel::send(STATE, OBJECT val) {
    if(!waiting->empty_p()) {
      Thread* thr = as<Thread>(waiting->shift(state));
      thr->set_top(state, val);
      state->queue_thread(thr);
      return Qnil;
    }

    if(value->nil_p()) {
      List* lst = List::create(state);
      lst->append(state, val);

      value = lst;
    }
    return Qnil;
  }

  OBJECT Channel::receive(STATE) {
    if(!value->nil_p()) {
      OBJECT val = as<List>(value)->shift(state);
      state->return_value(val);
      return Qnil;
    }

    /* We push nil on the stack to reserve a place to put the result. */
    state->return_value(Qfalse);

    G(current_thread)->sleep_for(state, this);
    waiting->append(state, G(current_thread));
    state->run_best_thread();
    return Qnil;
  }

  bool Channel::has_readers_p() {
    return !waiting->empty_p();
  }

  class SendToChannel : public ObjectCallback {
  public:
    TypedRoot<Channel*> chan;

    SendToChannel(STATE, Channel* chan) : ObjectCallback(state), chan(state, chan) { }

    virtual OBJECT object() {
      return chan.get();
    }

    virtual void call(OBJECT obj) {
      chan->send(state, obj);
    }
  };

  OBJECT Channel::send_on_signal(STATE, Channel* chan, FIXNUM signal) {
    SendToChannel* cb = new SendToChannel(state, chan);
    event::Signal* sig = new event::Signal(state, cb, signal->to_native());
    state->signal_events->start(sig);
    return signal;
  }

  OBJECT Channel::send_on_readable(STATE, Channel* chan, IO* io,
      IOBuffer* buffer, FIXNUM bytes) {

    SendToChannel* cb = new SendToChannel(state, chan);
    event::Read* sig = new event::Read(state, cb, io->to_fd());
    sig->into_buffer(buffer, bytes->to_native());

    state->events->start(sig);
    return io;
  }

  OBJECT Channel::send_in_microseconds(STATE, Channel* chan, Integer* useconds, OBJECT tag) {
    double seconds = useconds->to_native() / 1000000.0;

    return send_in_seconds(state, chan, seconds, tag);
  }

  OBJECT Channel::send_in_seconds(STATE, Channel* chan, Float* seconds, OBJECT tag) {
    return send_in_seconds(state, chan, seconds->to_double(state), tag);
  }

  OBJECT Channel::send_in_seconds(STATE, Channel* chan, double seconds, OBJECT tag) {
    SendToChannel* cb = new SendToChannel(state, chan);
    event::Timer* sig = new event::Timer(state, cb, seconds, tag);
    state->events->start(sig);
    return Qnil;
  }

  ChannelCallback::ChannelCallback(STATE, Channel* chan) : ObjectCallback(state) {
    channel.set(chan, &GO(roots));
  }

  void ChannelCallback::call(OBJECT obj) {
    channel->send(state, obj);
  }
}