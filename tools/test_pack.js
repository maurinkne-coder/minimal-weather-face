// Node check of the phone-side packing, with a fake Open-Meteo response.
var Module = require('module');
var orig = Module._load;
Module._load = function (req) {
  if (req === 'pebble-clay') return function () { this.generateUrl = function () {}; this.getSettings = function () {}; };
  return orig.apply(this, arguments);
};
global.Pebble = { addEventListener: function () {}, sendAppMessage: function () {} };
global.localStorage = { getItem: function () { return null; }, setItem: function () {}, removeItem: function () {} };
var app = require('../src/pkjs/index.js');
var assert = require('assert');

var now = Math.floor(Date.now() / 1000);
var h0 = now - (now % 3600) - 13 * 3600; // response starts at local midnight-ish
var hourly = { time: [], temperature_2m: [], precipitation: [], precipitation_probability: [], weather_code: [], wind_gusts_10m: [], uv_index: [], is_day: [] };
for (var i = 0; i < 120; i++) {
  hourly.time.push(h0 + i * 3600);
  hourly.temperature_2m.push(i === 13 ? -3.46 : 5.2);
  hourly.precipitation.push(i === 14 ? 1.24 : 0);
  hourly.precipitation_probability.push(i === 14 ? null : 20);
  hourly.weather_code.push(61);
  hourly.wind_gusts_10m.push(42.6);
  hourly.uv_index.push(i < 100 ? 3.14 : null);
  hourly.is_day.push(1);
}
var m0 = now - (now % 900) - 900;
var j = {
  current: { time: now - 120, temperature_2m: -1.5, weather_code: 3 },
  hourly: hourly,
  minutely_15: { time: [m0, m0 + 900, m0 + 1800, m0 + 2700, m0 + 3600, m0 + 4500, m0 + 5400, m0 + 6300, m0 + 7200, m0 + 8100], precipitation: [0, 0.2, 0, 0, 0, 0, 0, 0, 0, 0] },
  daily: { sunrise: [now + 1000, now + 87400, now + 173800, now + 260200, now + 346600], sunset: [now - 1000, now + 85400, now + 171800, now + 258200, now + 344600] }
};
var d = app.pack(j);
assert.strictEqual(d.WxT0, h0 + 13 * 3600);
assert.strictEqual(d.WxTemp.length, 72 * 2);
var t = d.WxTemp[0] | (d.WxTemp[1] << 8); if (t & 0x8000) t -= 0x10000;
assert.strictEqual(t, -35);
assert.strictEqual(d.WxPrecip[1], 12);
assert.strictEqual(d.WxProb[1], 255);
assert.strictEqual(d.WxGust[0], 43);
assert.strictEqual(d.WxUv[0], 31);
assert.strictEqual(d.WxM15T0, m0 + 900);
assert.deepStrictEqual(d.WxM15, [2, 0, 0, 0, 0, 0, 0, 0]);
assert.strictEqual(d.WxRise.length, 16);
assert.strictEqual(d.WxCurTemp, -15);
assert.ok(app.forecastUrl({ lat: 47.3769, lon: 8.5417 }, 'icon_seamless').indexOf('&models=icon_seamless') > 0);
// model without data -> null so the phone falls back to best match
hourly.temperature_2m = hourly.temperature_2m.map(function () { return null; });
assert.strictEqual(app.pack(j), null);
console.log('pack ok');
