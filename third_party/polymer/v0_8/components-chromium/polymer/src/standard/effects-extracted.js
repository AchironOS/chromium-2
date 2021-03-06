

  /**
   * Support for property side effects.
   *
   * Key for effect objects:
   *
   * property | ann | anCmp | cmp | obs | cplxOb | description
   * ---------|-----|-------|-----|-----|--------|----------------------------------------
   * method   |     | X     | X   | X   | X      | function name to call on instance
   * args     |     | X     | X   |     | X      | list of all arg descriptors for fn
   * arg      |     | X     | X   |     | X      | arg descriptor for effect
   * property |     |       | X   | X   |        | property for effect to set or get
   * name     | X   |       |     |     |        | annotation value (text inside {{...}})
   * kind     | X   | X     |     |     |        | binding type (property or attribute)
   * index    | X   | X     |     |     |        | node index to set
   *
   */

  Polymer.Base._addFeature({

    _addPropertyEffect: function(property, kind, effect) {
     // TODO(sjmiles): everything to the right of the first '.' is lost, implies
     // there is some duplicate information flow (not the only sign)
     var model = property.split('.').shift();
     Polymer.Bind.addPropertyEffect(this, model, kind, effect);
    },

    // prototyping

    _prepEffects: function() {
      Polymer.Bind.prepareModel(this);
      this._addAnnotationEffects(this._notes);
    },

    _prepBindings: function() {
      Polymer.Bind.createBindings(this);
    },

    _addPropertyEffects: function(effects) {
      if (effects) {
        for (var n in effects) {
          var effect = effects[n];
          if (effect.observer) {
            this._addObserverEffect(n, effect.observer);
          }
          if (effect.computed) {
            this._addComputedEffect(n, effect.computed);
          }
          if (effect.notify) {
            this._addPropertyEffect(n, 'notify');
          }
          if (effect.reflectToAttribute) {
            this._addPropertyEffect(n, 'reflect');
          }
          if (this.isReadOnlyProperty(n)) {
            // Ensure accessor is created
            Polymer.Bind.ensurePropertyEffects(this, n);
          }
        }
      }
    },

    _parseMethod: function(expression) {
      var m = expression.match(/(\w*)\((.*)\)/);
      if (m) {
        return {
          method: m[1],
          args: m[2].split(/[^\w.*]+/).map(this._parseArg)
        };
      }
    },

    _parseArg: function(arg) {
      var a = { name: arg };
      a.structured = arg.indexOf('.') > 0;
      if (a.structured) {
        a.wildcard = (arg.slice(-2) == '.*');
        if (a.wildcard) {
          a.name = arg.slice(0, -2);
        }
      }
      return a;
    },

    _addComputedEffect: function(name, expression) {
      var sig = this._parseMethod(expression);
      sig.args.forEach(function(arg) {
        this._addPropertyEffect(arg.name, 'compute', {
          method: sig.method,
          args: sig.args,
          arg: arg,
          property: name
        });
      }, this);
    },

    _addObserverEffect: function(property, observer) {
      this._addPropertyEffect(property, 'observer', {
        method: observer,
        property: property
      });
    },

    _addComplexObserverEffects: function(observers) {
      if (observers) {
        observers.forEach(function(observer) {
          this._addComplexObserverEffect(observer);
        }, this);
      }
    },

    _addComplexObserverEffect: function(observer) {
      var sig = this._parseMethod(observer);
      sig.args.forEach(function(arg) {
        this._addPropertyEffect(arg.name, 'complexObserver', {
          method: sig.method,
          args: sig.args,
          arg: arg
        });
      }, this);
    },

    _addAnnotationEffects: function(notes) {
      // create a virtual annotation list, must be concretized at instance time
      this._nodes = [];
      // process annotations that have been parsed from template
      notes.forEach(function(note) {
        // where to find the node in the concretized list
        var index = this._nodes.push(note) - 1;
        note.bindings.forEach(function(binding) {
          this._addAnnotationEffect(binding, index);
        }, this);
      }, this);
    },

    _addAnnotationEffect: function(note, index) {
      // TODO(sjmiles): annotations have 'effects' proper and 'listener'
      if (Polymer.Bind._shouldAddListener(note)) {
        // <node>.on.<dash-case-property>-changed: <path> = e.detail.value
        Polymer.Bind._addAnnotatedListener(this, index,
          note.name, note.value, note.event);
      }
      var sig = this._parseMethod(note.value);
      if (sig) {
        this._addAnnotatedComputationEffect(sig, note, index);
      } else {
        // capture the node index
        note.index = index;
        // discover top-level property (model) from path
        var model = note.value.split('.').shift();
        // add 'annotation' binding effect for property 'model'
        this._addPropertyEffect(model, 'annotation', note);
      }
    },

    _addAnnotatedComputationEffect: function(sig, note, index) {
      sig.args.forEach(function(arg) {
        this._addPropertyEffect(arg.name, 'annotatedComputation', {
          kind: note.kind,
          method: sig.method,
          args: sig.args,
          arg: arg,
          property: note.name,
          index: index
        });
      }, this);
    },

    // instancing

    _marshalInstanceEffects: function() {
      Polymer.Bind.prepareInstance(this);
      Polymer.Bind.setupBindListeners(this);
    },

    _applyEffectValue: function(value, info) {
      var node = this._nodes[info.index];
      // TODO(sorvell): ideally, the info object is normalized for easy
      // lookup here.
      var property = info.property || info.name || 'textContent';
      // special processing for 'class' and 'className'; 'class' handled
      // when attr is serialized.
      if (info.kind == 'attribute') {
        this.serializeValueToAttribute(value, property, node);
      } else {
        // TODO(sorvell): consider pre-processing this step so we don't need
        // this lookup.
        if (property === 'className') {
          value = this._scopeElementClass(node, value);
        }
        return node[property] = value;
      }
    }

  });

