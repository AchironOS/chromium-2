

Polymer.Debounce = (function() {
  
  // usage
  
  // invoke cb.call(this) in 100ms, unless the job is re-registered,
  // which resets the timer
  // 
  // this.job = this.debounce(this.job, cb, 100)
  //
  // returns a handle which can be used to re-register a job

  var Async = Polymer.Async;
  
  var Debouncer = function(context) {
    this.context = context;
    this.boundComplete = this.complete.bind(this);
  };
  
  Debouncer.prototype = {
    go: function(callback, wait) {
      var h;
      this.finish = function() {
        Async.cancel(h);
      };
      h = Async.run(this.boundComplete, wait);
      this.callback = callback;
    },
    stop: function() {
      if (this.finish) {
        this.finish();
        this.finish = null;
      }
    },
    complete: function() {
      if (this.finish) {
        this.stop();
        this.callback.call(this.context);
      }
    }
  };

  function debounce(debouncer, callback, wait) {
    if (debouncer) {
      debouncer.stop();
    } else {
      debouncer = new Debouncer(this);
    }
    debouncer.go(callback, wait);
    return debouncer;
  }
  
  // exports 

  return debounce;
  
})();

