

  (function() {

    function toCssText(rules, callback) {
      if (typeof rules === 'string') {
        rules = Polymer.CssParse.parse(rules);
      } 
      if (callback) {
        forEachStyleRule(rules, callback);
      }
      return Polymer.CssParse.stringify(rules);
    }

    function forEachStyleRule(node, cb) {
      var s = node.selector;
      var skipRules = false;
      if (s) {
        if ((s.indexOf(AT_RULE) !== 0) && (s.indexOf(MIXIN_SELECTOR) !== 0)) {
          cb(node);
        }
        skipRules = (s.indexOf(KEYFRAME_RULE) >= 0) || 
          (s.indexOf(MIXIN_SELECTOR) >= 0);
      }
      var r$ = node.rules;
      if (r$ && !skipRules) {
        for (var i=0, l=r$.length, r; (i<l) && (r=r$[i]); i++) {
          forEachStyleRule(r, cb);
        }
      }
    }

    // add a string of cssText to the document.
    function applyCss(cssText, moniker, target, lowPriority) {
      var style = document.createElement('style');
      if (moniker) {
        style.setAttribute('scope', moniker);
      }
      style.textContent = cssText;
      target = target || document.head;
      if (lowPriority) {
        var n$ = target.querySelectorAll('style[scope]');
        var ref = n$.length ? n$[n$.length-1].nextSibling : target.firstChild;
        target.insertBefore(style, ref);
     } else {
        target.appendChild(style);
      }
      return style;
    }

    var AT_RULE = '@';
    var KEYFRAME_RULE = 'keyframe';
    var MIXIN_SELECTOR = '--';

    // exports
    Polymer.StyleUtil = {
      parser: Polymer.CssParse,
      applyCss: applyCss,
      forEachStyleRule: forEachStyleRule,
      toCssText: toCssText
    };

  })();

