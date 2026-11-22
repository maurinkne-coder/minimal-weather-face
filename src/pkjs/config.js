// Settings page (Clay). Values are strings for selects/radios, booleans for
// toggles; index.js turns them into numbers for the watch.
module.exports = [
  {
    type: 'heading',
    defaultValue: 'Contour'
  },
  {
    type: 'text',
    defaultValue: 'A big clock with the coming rain drawn as bars round the edge.'
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Look' },
      {
        type: 'radiogroup',
        messageKey: 'Theme',
        label: 'Theme',
        defaultValue: '0',
        options: [
          { label: 'Midnight (black)', value: '0' },
          { label: 'Daylight (white, easy to read outdoors)', value: '1' },
          { label: 'Mono (black and white only)', value: '2' },
          { label: 'Graphite (dark grey)', value: '3' },
          { label: 'Ocean (deep blue)', value: '5' },
          { label: 'Forest (dark green)', value: '6' },
          { label: 'Ember (black with warm orange text)', value: '7' },
          { label: 'Sand (warm light, easy to read outdoors)', value: '8' },
          { label: 'Automatic (Daylight by day, Midnight at night)', value: '4' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'OwnColors',
        label: 'Own text and accent colours',
        description: 'Replaces the palette\'s colours for the clock and the smart line. The rain stays blue. Not used with Mono.',
        defaultValue: false
      },
      {
        type: 'select',
        messageKey: 'TextColor',
        label: 'Text colour (clock)',
        defaultValue: 'FFFFFF',
        options: [
          { label: 'White', value: 'FFFFFF' },
          { label: 'Light grey', value: 'AAAAAA' },
          { label: 'Ice blue', value: 'AAFFFF' },
          { label: 'Mint', value: 'AAFFAA' },
          { label: 'Lemon', value: 'FFFF55' },
          { label: 'Peach', value: 'FFAA55' },
          { label: 'Rose', value: 'FFAAAA' },
          { label: 'Lavender', value: 'AAAAFF' },
          { label: 'Black (for light palettes)', value: '000000' },
          { label: 'Navy (for light palettes)', value: '000055' }
        ]
      },
      {
        type: 'select',
        messageKey: 'AccentColor',
        label: 'Accent colour (smart line)',
        defaultValue: '55FFFF',
        options: [
          { label: 'Cyan', value: '55FFFF' },
          { label: 'Mint', value: 'AAFFAA' },
          { label: 'Green', value: '55FF55' },
          { label: 'Yellow', value: 'FFFF00' },
          { label: 'Orange', value: 'FFAA00' },
          { label: 'Coral', value: 'FF5555' },
          { label: 'Pink', value: 'FF55AA' },
          { label: 'Violet', value: 'AA55FF' },
          { label: 'White', value: 'FFFFFF' },
          { label: 'Blue (for light palettes)', value: '0055AA' },
          { label: 'Dark red (for light palettes)', value: 'AA0000' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Range',
        label: 'Line length (Pebble Time 2)',
        description: 'On Round 2 the ring always shows 24 hours.',
        defaultValue: '24',
        options: [
          { label: '12 hours', value: '12' },
          { label: '24 hours', value: '24' },
          { label: '48 hours', value: '48' }
        ]
      },
      {
        type: 'select',
        messageKey: 'SymLeft',
        label: 'Symbol left of the middle text',
        defaultValue: '1',
        options: [
          { label: 'Off', value: '0' },
          { label: 'Weather now (sun, clouds, rain, snow…)', value: '1' },
          { label: 'Rain chance (droplet fills up)', value: '2' },
          { label: 'Phone disconnected (only when it is)', value: '3' },
          { label: 'Quiet Time (only when it is on)', value: '4' },
          { label: 'Wind direction (arrow, highlighted in strong gusts)', value: '5' },
          { label: 'Moon phase', value: '6' },
          { label: 'Heart rate (red when above 100 bpm)', value: '7' }
        ]
      },
      {
        type: 'select',
        messageKey: 'SymRight',
        label: 'Symbol right of the middle text',
        description: 'While the watch charges, a lightning bolt takes this place.',
        defaultValue: '2',
        options: [
          { label: 'Off', value: '0' },
          { label: 'Weather now (sun, clouds, rain, snow…)', value: '1' },
          { label: 'Rain chance (droplet fills up)', value: '2' },
          { label: 'Phone disconnected (only when it is)', value: '3' },
          { label: 'Quiet Time (only when it is on)', value: '4' },
          { label: 'Wind direction (arrow, highlighted in strong gusts)', value: '5' },
          { label: 'Moon phase', value: '6' },
          { label: 'Heart rate (red when above 100 bpm)', value: '7' }
        ]
      },
      {
        type: 'select',
        messageKey: 'BarSlot',
        label: 'Rain bar resolution',
        description: 'How much time each rain bar covers. Shorter bars show more detail in the next two hours, where a 15-minute forecast is available.',
        defaultValue: '30',
        options: [
          { label: '15 minutes (finest)', value: '15' },
          { label: '30 minutes', value: '30' },
          { label: '1 hour (calmest)', value: '60' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Corner',
        label: 'Bottom line',
        description: 'Small text at the very bottom, under the minutes.',
        defaultValue: '4',
        options: [
          { label: 'Nothing', value: '0' },
          { label: 'Temperature: low, now, high today', value: '4' },
          { label: 'Battery', value: '1' },
          { label: 'Steps today', value: '2' },
          { label: 'Heart rate', value: '3' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'Anim',
        label: 'Animations',
        description: 'The chart draws itself when the face opens; a wrist flick walks through the coming hours. Always off in Quiet Time and below 20% battery.',
        defaultValue: true
      }
    ]
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Tilt' },
      {
        type: 'select',
        messageKey: 'Tilt',
        label: 'Tilt',
        description: 'Turns the whole face, for example if you wear the watch on the inside of your wrist or look at it from an angle while cycling.',
        defaultValue: '0',
        options: [
          { label: 'Upright', value: '0' },
          { label: '15° clockwise', value: '15' },
          { label: '30° clockwise', value: '30' },
          { label: '45° clockwise', value: '45' },
          { label: '60° clockwise', value: '60' },
          { label: '90° clockwise', value: '90' },
          { label: '15° anticlockwise', value: '-15' },
          { label: '30° anticlockwise', value: '-30' },
          { label: '45° anticlockwise', value: '-45' },
          { label: '60° anticlockwise', value: '-60' },
          { label: '90° anticlockwise', value: '-90' },
          { label: 'Upside down', value: '180' }
        ]
      }
    ]
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Battery and steps' },
      {
        type: 'select',
        messageKey: 'BattLine',
        label: 'Battery line',
        description: 'A thin line just above the middle text that shrinks as the watch battery runs down.',
        defaultValue: '1',
        options: [
          { label: 'Off', value: '0' },
          { label: 'Plain (red at 20% or less)', value: '1' },
          { label: 'Colourful (green, orange below 30%, red below 10%)', value: '2' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'ChargeBuzz',
        label: 'Charging buzz',
        description: 'Buzzes when you plug the watch in, at 80% and when it is full. The charging animation and lightning bolt show either way. Silent in Quiet Time.',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'Buzz80',
        label: 'Buzzes at 80%',
        defaultValue: '3',
        options: [
          { label: 'Off', value: '0' },
          { label: '1 buzz', value: '1' },
          { label: '2 buzzes', value: '2' },
          { label: '3 buzzes', value: '3' },
          { label: '4 buzzes', value: '4' },
          { label: '5 buzzes', value: '5' },
          { label: '6 buzzes', value: '6' },
          { label: '7 buzzes', value: '7' },
          { label: '8 buzzes', value: '8' },
          { label: '9 buzzes', value: '9' },
          { label: '10 buzzes', value: '10' }
        ]
      },
      {
        type: 'select',
        messageKey: 'BuzzFull',
        label: 'Buzzes when full',
        defaultValue: '5',
        options: [
          { label: 'Off', value: '0' },
          { label: '1 buzz', value: '1' },
          { label: '2 buzzes', value: '2' },
          { label: '3 buzzes', value: '3' },
          { label: '4 buzzes', value: '4' },
          { label: '5 buzzes', value: '5' },
          { label: '6 buzzes', value: '6' },
          { label: '7 buzzes', value: '7' },
          { label: '8 buzzes', value: '8' },
          { label: '9 buzzes', value: '9' },
          { label: '10 buzzes', value: '10' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'StepBar',
        label: 'Step goal bar',
        description: 'A bar just below the middle text that fills up as you walk towards your step goal.',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'StepGoal',
        label: 'Step goal',
        defaultValue: '10000',
        options: [
          { label: 'My usual day (from Pebble Health)', value: '0' },
          { label: '5,000', value: '5000' },
          { label: '7,500', value: '7500' },
          { label: '10,000', value: '10000' },
          { label: '12,500', value: '12500' },
          { label: '15,000', value: '15000' },
          { label: 'My own number (below)', value: 'own' }
        ]
      },
      {
        type: 'input',
        messageKey: 'StepGoalOwn',
        label: 'My own step goal',
        description: 'Used when the step goal above is set to "My own number". Between 100 and 60,000.',
        defaultValue: '8000',
        attributes: { type: 'number', min: 100, max: 60000, step: 100, placeholder: 'e.g. 8000' }
      },
      {
        type: 'select',
        messageKey: 'StepColor',
        label: 'Step line colour',
        description: 'Never blue, so it can\'t be mistaken for rain.',
        defaultValue: '0',
        options: [
          { label: 'Theme colour', value: '0' },
          { label: 'Yellow', value: '1' },
          { label: 'Orange', value: '2' },
          { label: 'Red', value: '3' },
          { label: 'Pink', value: '4' },
          { label: 'Violet', value: '5' },
          { label: 'Green', value: '6' },
          { label: 'Mint', value: '7' },
          { label: 'White', value: '8' }
        ]
      }
    ]
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Smart line' },
      {
        type: 'text',
        defaultValue: 'One line with the most important thing, in this order: rain now or within 2 hours, frost tonight, wind gusts, UV, "dry until", then the next sunset or sunrise.'
      },
      {
        type: 'select',
        messageKey: 'RainAlert',
        label: 'Rain warning',
        description: 'A short double buzz before rain starts, once per shower. Silent in Quiet Time.',
        defaultValue: '0',
        options: [
          { label: 'Off', value: '0' },
          { label: '15 minutes before', value: '15' },
          { label: '30 minutes before', value: '30' },
          { label: '60 minutes before', value: '60' }
        ]
      },
      { type: 'toggle', messageKey: 'Frost', label: 'Frost tonight', defaultValue: true },
      {
        type: 'select',
        messageKey: 'Gust',
        label: 'Wind gusts from',
        defaultValue: '50',
        options: [
          { label: 'Off', value: '0' },
          { label: '40 km/h (25 mph, 22 kn, 6 Bft)', value: '40' },
          { label: '50 km/h (31 mph, 27 kn, 7 Bft)', value: '50' },
          { label: '60 km/h (37 mph, 32 kn, 7 Bft)', value: '60' },
          { label: '70 km/h (43 mph, 38 kn, 8 Bft)', value: '70' },
          { label: '80 km/h (50 mph, 43 kn, 9 Bft)', value: '80' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Uv',
        label: 'UV index from',
        defaultValue: '6',
        options: [
          { label: 'Off', value: '0' },
          { label: '3', value: '3' },
          { label: '4', value: '4' },
          { label: '5', value: '5' },
          { label: '6', value: '6' },
          { label: '7', value: '7' },
          { label: '8', value: '8' },
          { label: '9', value: '9' },
          { label: '10', value: '10' },
          { label: '11', value: '11' }
        ]
      },
      { type: 'toggle', messageKey: 'SunLine', label: 'Sunset and sunrise', defaultValue: true }
    ]
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Language and units' },
      {
        type: 'select',
        messageKey: 'Lang',
        label: 'Language',
        defaultValue: '0',
        options: [
          { label: 'English', value: '0' },
          { label: 'Deutsch', value: '1' },
          { label: 'Français', value: '2' },
          { label: 'Italiano', value: '3' },
          { label: 'Español', value: '4' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Clock',
        label: 'Clock',
        defaultValue: '0',
        options: [
          { label: 'Automatic (watch setting)', value: '0' },
          { label: '24 hour', value: '1' },
          { label: '12 hour', value: '2' }
        ]
      },
      {
        type: 'select',
        messageKey: 'TempUnit',
        label: 'Temperature',
        defaultValue: '0',
        options: [
          { label: '°C', value: '0' },
          { label: '°F', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'RainUnit',
        label: 'Rain',
        defaultValue: '0',
        options: [
          { label: 'mm', value: '0' },
          { label: 'inch', value: '1' }
        ]
      },
      {
        type: 'select',
        messageKey: 'WindUnit',
        label: 'Wind speed',
        defaultValue: '0',
        options: [
          { label: 'km/h', value: '0' },
          { label: 'mph', value: '1' },
          { label: 'm/s', value: '2' },
          { label: 'knots', value: '3' },
          { label: 'Beaufort', value: '4' }
        ]
      }
    ]
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Weather' },
      {
        type: 'toggle',
        messageKey: 'Updates',
        label: 'Weather updates',
        description: 'From Open-Meteo, no account needed. The watch keeps 3 days, so the track keeps moving when your phone is away. No updates below 20% battery.',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'Interval',
        label: 'Update every',
        defaultValue: '30',
        options: [
          { label: '15 minutes', value: '15' },
          { label: '30 minutes', value: '30' },
          { label: '60 minutes', value: '60' }
        ]
      },
      {
        type: 'select',
        messageKey: 'Model',
        label: 'Weather model',
        defaultValue: 'best_match',
        options: [
          { label: 'Best match (Open-Meteo picks)', value: 'best_match' },
          { label: 'MeteoSwiss ICON CH', value: 'meteoswiss_icon_ch2' },
          { label: 'DWD ICON', value: 'icon_seamless' },
          { label: 'ECMWF', value: 'ecmwf_ifs025' },
          { label: 'Météo-France', value: 'meteofrance_seamless' },
          { label: 'UK Met Office', value: 'ukmo_seamless' },
          { label: 'NOAA GFS', value: 'gfs_seamless' }
        ]
      },
      {
        type: 'radiogroup',
        messageKey: 'LocMode',
        label: 'Location',
        defaultValue: 'gps',
        options: [
          { label: 'GPS (phone location)', value: 'gps' },
          { label: 'City below', value: 'city' }
        ]
      },
      {
        type: 'input',
        messageKey: 'City',
        label: 'City',
        defaultValue: '',
        attributes: { placeholder: 'e.g. Zürich' }
      },
      {
        type: 'text',
        defaultValue: 'Your location is only used to ask Open-Meteo for the forecast.'
      }
    ]
  },

  { type: 'submit', defaultValue: 'Save' }
];
