// Runs inside the settings page: hides settings that only matter when
// another one is switched on, so the page stays short.
module.exports = function () {
  var clayConfig = this;

  function link(controlKey, keys, isOn) {
    var control = clayConfig.getItemByMessageKey(controlKey);
    if (!control) return;
    function update() {
      var on = isOn(control.get());
      keys.forEach(function (k) {
        var item = clayConfig.getItemByMessageKey(k);
        if (item) { if (on) item.show(); else item.hide(); }
      });
    }
    update();
    control.on('change', update);
  }

  function truthy(v) { return v === true || v === 'true' || v === 1; }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    link('OwnColors', ['TextColor', 'AccentColor'], truthy);
    link('ChargeBuzz', ['Buzz80', 'BuzzFull'], truthy);
    link('StepBar', ['StepGoal', 'StepColor'], truthy);
    link('StepGoal', ['StepGoalOwn'], function (v) { return v === 'own'; });
    link('Updates', ['Interval', 'Model'], truthy);
    link('LocMode', ['City'], function (v) { return v === 'city'; });
  });
};
