// Phone side: settings page, location, and the Open-Meteo forecast.
var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var HOURS = 72;
var M15_SLOTS = 8;
var SUN_DAYS = 4;

var DEFAULTS = {
  Theme: '0', Range: '24', Corner: '4', Anim: true,
  Frost: true, Gust: '50', Uv: '6', SunLine: true,
  Lang: '0', Clock: '0', TempUnit: '0', RainUnit: '0', WindUnit: '0',
  Updates: true, Interval: '30', Tilt: '0', BattLine: '1', StepBar: true, StepGoal: '10000', StepColor: '0', RainAlert: '0',
  OwnColors: false, TextColor: 0xFFFFFF, AccentColor: 0x55FFFF, Model: 'best_match', LocMode: 'gps', City: ''
};

var WATCH_KEYS = ['Theme', 'Range', 'Corner', 'Anim', 'Frost', 'Gust', 'Uv', 'SunLine',
                  'Lang', 'Clock', 'TempUnit', 'RainUnit', 'WindUnit', 'Updates', 'Interval', 'Tilt',
                  'BattLine', 'StepBar', 'StepGoal', 'StepColor', 'RainAlert',
                  'OwnColors', 'TextColor', 'AccentColor'];

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

function loadSettings() {
  var s = {};
  var stored = {};
  try {
    stored = JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (e) { stored = {}; }
  for (var k in DEFAULTS) {
    var v = stored[k];
    if (v && typeof v === 'object' && 'value' in v) v = v.value;
    s[k] = (v === undefined || v === null) ? DEFAULTS[k] : v;
  }
  return s;
}

function toInt(v) {
  if (typeof v === 'string' && /^(0x|#)?[0-9a-f]{6}$/i.test(v)) return parseInt(v.replace('#', ''), 16); // colour
  if (v === true) return 1;
  if (v === false) return 0;
  var n = parseInt(v, 10);
  return isNaN(n) ? 0 : n;
}

// ---------------------------------------------------------------------------
// Sending to the watch, one message at a time
// ---------------------------------------------------------------------------

var queue = [];
var sending = false;

function send(dict) {
  queue.push({ dict: dict, tries: 0 });
  pump();
}

function pump() {
  if (sending || !queue.length) return;
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(item.dict, function () {
    queue.shift();
    sending = false;
    pump();
  }, function () {
    sending = false;
    item.tries++;
    if (item.tries >= 3) queue.shift();
    setTimeout(pump, 1000);
  });
}

function sendSettings(s) {
  var dict = {};
  WATCH_KEYS.forEach(function (k) { dict[k] = toInt(s[k]); });
  send(dict);
}

// ---------------------------------------------------------------------------
// Location
// ---------------------------------------------------------------------------

function cached(key) {
  try { return JSON.parse(localStorage.getItem(key)); } catch (e) { return null; }
}

function getJSON(url, ok, fail) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  xhr.timeout = 20000;
  xhr.onload = function () {
    if (xhr.status >= 200 && xhr.status < 300) {
      try { ok(JSON.parse(xhr.responseText)); } catch (e) { fail(); }
    } else {
      fail();
    }
  };
  xhr.onerror = fail;
  xhr.ontimeout = fail;
  xhr.send();
}

function getLocation(s, done) {
  if (s.LocMode === 'city' && s.City && String(s.City).trim()) {
    var name = String(s.City).trim();
    var c = cached('city-loc');
    if (c && c.name === name) return done(c);
    getJSON('https://geocoding-api.open-meteo.com/v1/search?count=1&format=json&name=' +
            encodeURIComponent(name), function (j) {
      if (j && j.results && j.results.length) {
        var loc = { name: name, lat: j.results[0].latitude, lon: j.results[0].longitude };
        localStorage.setItem('city-loc', JSON.stringify(loc));
        done(loc);
      } else {
        done(null, 'location');
      }
    }, function () { done(null, 'network'); });
    return;
  }

  navigator.geolocation.getCurrentPosition(function (pos) {
    var loc = { lat: pos.coords.latitude, lon: pos.coords.longitude };
    localStorage.setItem('gps-loc', JSON.stringify(loc));
    done(loc);
  }, function () {
    var last = cached('gps-loc');
    if (last) done(last); else done(null, 'location');
  }, { enableHighAccuracy: false, timeout: 15000, maximumAge: 15 * 60 * 1000 });
}

// ---------------------------------------------------------------------------
// Forecast
// ---------------------------------------------------------------------------

function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
function num(v, fallback) { return (v === null || v === undefined || isNaN(v)) ? fallback : v; }

function u32(arr) {
  var out = [];
  arr.forEach(function (v) {
    out.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
  });
  return out;
}

// Index of the last entry at or before `now`.
function startIndex(times, now) {
  var i = 0;
  while (i + 1 < times.length && times[i + 1] <= now) i++;
  return i;
}

function forecastUrl(loc, model) {
  return 'https://api.open-meteo.com/v1/forecast' +
    '?latitude=' + loc.lat.toFixed(3) + '&longitude=' + loc.lon.toFixed(3) +
    '&current=temperature_2m,weather_code' +
    '&hourly=temperature_2m,precipitation,precipitation_probability,weather_code,wind_gusts_10m,uv_index,is_day' +
    '&minutely_15=precipitation' +
    '&daily=sunrise,sunset,temperature_2m_max,temperature_2m_min' +
    '&timezone=auto&timeformat=unixtime&forecast_days=5' +
    (model && model !== 'best_match' ? '&models=' + encodeURIComponent(model) : '');
}

function pack(j) {
  var now = Math.floor(Date.now() / 1000);
  var h = j.hourly;
  var s = startIndex(h.time, now);
  var end = Math.min(h.time.length, s + HOURS);

  var temp = [], precip = [], prob = [], code = [], gust = [], uv = [], day = [];
  for (var i = s; i < end; i++) {
    var t = h.temperature_2m[i];
    if (t === null || t === undefined) break; // model ran out of hours
    var t10 = Math.round(t * 10) & 0xffff;
    temp.push(t10 & 0xff, (t10 >> 8) & 0xff);
    precip.push(clamp(Math.round(num(h.precipitation[i], 0) * 10), 0, 254));
    var p = h.precipitation_probability ? h.precipitation_probability[i] : null;
    prob.push(p === null || p === undefined ? 255 : clamp(Math.round(p), 0, 100));
    code.push(clamp(num(h.weather_code[i], 3), 0, 99));
    gust.push(clamp(Math.round(num(h.wind_gusts_10m[i], 0)), 0, 254));
    uv.push(clamp(Math.round(num(h.uv_index ? h.uv_index[i] : 0, 0) * 10), 0, 254));
    day.push(num(h.is_day[i], 1) ? 1 : 0);
  }
  if (!temp.length) return null;

  var dict = {
    WxT0: h.time[s],
    WxTemp: temp, WxPrecip: precip, WxProb: prob, WxCode: code,
    WxGust: gust, WxUv: uv, WxDay: day
  };

  var m = j.minutely_15;
  if (m && m.time && m.precipitation) {
    // Start at the current half hour so the watch can pair quarters into 30-minute bars.
    var ms = startIndex(m.time, now - (now % 1800));
    var slots = [];
    var known = false;
    for (var k = ms; k < m.time.length && slots.length < M15_SLOTS; k++) {
      if (m.precipitation[k] !== null && m.precipitation[k] !== undefined) known = true;
      slots.push(clamp(Math.round(num(m.precipitation[k], 0) * 10), 0, 254));
    }
    if (known && slots.length) {
      dict.WxM15T0 = m.time[ms];
      dict.WxM15 = slots;
    }
  }

  var d = j.daily;
  if (d && d.sunrise && d.sunset) {
    dict.WxRise = u32(d.sunrise.slice(0, SUN_DAYS).map(function (v) { return v || 0; }));
    dict.WxSet = u32(d.sunset.slice(0, SUN_DAYS).map(function (v) { return v || 0; }));
  }

  if (d && d.time && d.temperature_2m_max && d.temperature_2m_min) {
    var hi = [], lo = [];
    for (var di = 0; di < SUN_DAYS && di < d.time.length; di++) {
      var mx = d.temperature_2m_max[di], mn = d.temperature_2m_min[di];
      if (mx === null || mx === undefined || mn === null || mn === undefined) break;
      var a = Math.round(mx * 10) & 0xffff, b = Math.round(mn * 10) & 0xffff;
      hi.push(a & 0xff, (a >> 8) & 0xff);
      lo.push(b & 0xff, (b >> 8) & 0xff);
    }
    if (hi.length) {
      dict.WxDay0 = d.time[0];
      dict.WxHi = hi;
      dict.WxLo = lo;
    }
  }

  if (j.current && j.current.temperature_2m !== null && j.current.temperature_2m !== undefined) {
    dict.WxCurTemp = Math.round(j.current.temperature_2m * 10);
    dict.WxCurCode = num(j.current.weather_code, 3);
    dict.WxCurT = j.current.time;
  }
  return dict;
}

var fetching = false;

function fetchWeather() {
  var s = loadSettings();
  if (!toInt(s.Updates) || fetching) return;
  fetching = true;
  getLocation(s, function (loc, err) {
    if (!loc) {
      fetching = false;
      send({ WxStatus: err === 'network' ? 2 : 1 });
      return;
    }
    var tryModel = function (model, fallback) {
      getJSON(forecastUrl(loc, model), function (j) {
        var dict = null;
        try { dict = pack(j); } catch (e) { dict = null; }
        if (!dict && fallback) return tryModel('best_match', false); // model has no data here
        fetching = false;
        if (dict) send(dict); else send({ WxStatus: 2 });
      }, function () {
        fetching = false;
        send({ WxStatus: 2 });
      });
    };
    tryModel(s.Model, s.Model !== 'best_match');
  });
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  send({ JsReady: 1 });
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && (e.payload.Request || e.payload.Request === 0)) fetchWeather();
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  clay.getSettings(e.response); // stores the values in localStorage
  var s = loadSettings();
  sendSettings(s);
  localStorage.removeItem('city-loc');
  fetchWeather();
});

module.exports = { pack: pack, startIndex: startIndex, forecastUrl: forecastUrl };
