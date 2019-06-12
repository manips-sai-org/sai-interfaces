import { get_redis_val, post_redis_key_val } from '../redis.js';


const template = document.createElement('template');
template.innerHTML = `
  <style>
    .sai2-interface-slider-top {
      display: flex;
      flex-direction: column;
      flex-wrap: wrap;
      align-items: center;
    }

    .sai2-interface-slider-top div {
      width: 100%;
    }

    .sai2-interface-slider-top div div {
      display: flex;
      justify-content: space-between;
      align-items: center;
      width: 100%;
    }

    .sai2-interface-slider-top div div p {
      flex: 1;
    }

    .sai2-interface-slider-top div div input {
      width: 50%;
    }

  </style>
  <div class="sai2-interface-slider-top">
  </div>
`;

customElements.define('sai2-interface-slider', class extends HTMLElement {
  constructor() {
    super();
    this.template = template;
  }

  connectedCallback() {
    let template_node = this.template.content.cloneNode(true);
    this.key = this.getAttribute('key');
    this.min = this.getAttribute('min');
    this.max = this.getAttribute('max');
    this.step = this.getAttribute('step');

    // if we can parse as a JSON array, attempt to do so
    let raw_disp = this.getAttribute('display');
    try {
      this.display = JSON.parse(raw_disp);
      this.min = JSON.parse(this.min);
      this.max = JSON.parse(this.max);
    } catch (e) {
      this.display = raw_disp;
    }

    // create sliders
    let container = template_node.querySelector('div');
    get_redis_val(this.key).then(value => {
      // determine iteration bounds: 1 if scalar key, array size if vector
      let len = (Array.isArray(value)) ? value.length : 1;

      // save value
      this.value = value;

      // generate appropriate number of sliders
      for (let i = 0; i < len; i++) {
        /** 
         * The following js should be equivalent of this:
         * <div>
         *   <div>
         * 	   <label>item name</label>
         * 	   <input type="number" class="number" onkeydown="return false">
         *   </div>
         *   <input type="range" class="slider">
         * <div>
         */

        let slider_div = document.createElement('div');
        let slider_value_div = document.createElement('div');
        let slider_display = document.createElement('label');
        let slider_value_input = document.createElement('input');
        let slider = document.createElement('input');

        // set up slider name
        if (Array.isArray(value)) {
          if (Array.isArray(this.display)) {
            slider_display.innerHTML = this.display[i];
          } else {
            slider_display.innerHTML = (this.display || this.key) + "[" + i + "]";
          }
        } else {
          slider_display.innerHTML = this.display || this.key;
        }

        // set up manual value input for this slider
        slider_value_input.type = 'number';
        slider_value_input.className = 'value';
        slider_value_input.min = (Array.isArray(this.min)) ? this.min[i] : this.min;
        slider_value_input.max = (Array.isArray(this.max)) ? this.max[i] : this.max;
        slider_value_input.step = (Array.isArray(this.step)) ? this.step[i] : this.step;
        slider_value_input.value = (Array.isArray(value)) ? value[i] : value;

        // set up typing event
        let typingTimer;
        let sliding_value_input_callback = () => {
          let slider_val = parseFloat(slider_value_input.value);
          if (slider_val < slider_value_input.min)
            slider_val = slider_value_input.min;
          if (slider_val > slider_value_input.max)
            slider_val = slider_value_input.max;

          // HTML min/max coerces back to string, unfortunately
          slider_val = parseFloat(slider_val);
          slider_value_input.value = slider_val;
          slider.value = slider_val;

          if (Array.isArray(this.value))
            this.value[i] = slider_val;
          else
            this.value = slider_val;

          post_redis_key_val(this.key, this.value);
        }

        // wait for 250ms for user to stop typing before issuing redis write
        slider_value_input.addEventListener('keyup', () => {
          clearTimeout(typingTimer);
          if (slider_value_input.value)
            typingTimer = setTimeout(sliding_value_input_callback, 500);
        });

        // set up drag slider
        slider.type = 'range';
        slider.className = 'slider';
        slider.min = (Array.isArray(this.min)) ? this.min[i] : this.min;
        slider.max = (Array.isArray(this.max)) ? this.max[i] : this.max;
        slider.step = (Array.isArray(this.step)) ? this.step[i] : this.step;
        slider.value = (Array.isArray(value)) ? value[i] : value;
        slider.oninput = () => {
          let slider_val = parseFloat(slider.value);
          if (Array.isArray(this.value))
            this.value[i] = slider_val;
          else
            this.value = slider_val;

          slider_value_input.value = slider_val;

          post_redis_key_val(this.key, this.value);
        };

        // append label + manual value input to slider_value_div
        slider_value_div.appendChild(slider_display);
        slider_value_div.appendChild(slider_value_input);
        
        // add them all together
        slider_div.append(slider_value_div);
        slider_div.append(slider);
        container.append(slider_div);
      }
    });

      // append to document
      this.appendChild(template_node);
  }
});