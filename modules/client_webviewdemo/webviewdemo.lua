local webViewWindow
local webViewButton
local webView

local html = [[
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8"/>
  <style>
    body { font-family: Arial, sans-serif; background:#f2f2f2; margin:20px; line-height: 1.6; }
    h1 { color:#0080ff; }
    .container {
      background: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
      max-width: 600px;
    }
    label { display: block; margin-bottom: 5px; font-weight: bold; color: #333; }
    input, textarea {
      width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;
      font-size: 14px; box-sizing: border-box;
    }
    input:focus, textarea:focus {
      outline: none; border-color: #0080ff; box-shadow: 0 0 5px rgba(0,128,255,0.3);
    }
    button { padding: 8px 16px; margin: 5px; background: #0080ff; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
    button:hover { background: #0066cc; }
    .status { margin-top: 15px; padding: 10px; background: #e8f4fd; border-radius: 4px; border-left: 4px solid #0080ff; }
    .keylog { background: #f9f9f9; padding: 10px; border-radius: 4px; font-family: monospace; font-size: 12px; max-height: 100px; overflow-y: auto; }
    .debug-section { background: #fff3cd; border: 1px solid #ffeaa7; border-radius: 4px; padding: 10px; margin: 10px 0; }

    .custom-dropdown { position: relative; width: 100%; }
    .dropdown-button {
      width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;
      background: white; text-align: left; cursor: pointer; font-size: 14px;
      display: flex; justify-content: space-between; align-items: center;
      color: #333;
    }
    .dropdown-button:hover { border-color: #0080ff; }
    .dropdown-arrow {
      border: solid #666; border-width: 0 2px 2px 0;
      display: inline-block; padding: 3px; transform: rotate(45deg);
      transition: transform 0.2s;
    }
    .dropdown-open .dropdown-arrow { transform: rotate(-135deg); }
    .dropdown-options {
      position: absolute; top: 100%; left: 0; right: 0; background: white;
      border: 1px solid #ddd; border-top: none; border-radius: 0 0 4px 4px;
      max-height: 200px; overflow-y: auto; z-index: 1000; display: none;
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
    }
    .dropdown-open .dropdown-options { display: block; }
    .dropdown-option { padding: 8px 12px; cursor: pointer; border-bottom: 1px solid #f0f0f0; }
    .dropdown-option:hover { background: #f5f5f5; }
    .dropdown-option.selected { background: #e3f2fd; color: #0080ff; }
    .dropdown-option:last-child { border-bottom: none; }
  </style>
</head>
<body>
  <div class="container">
    <h1>Keyboard Input Test</h1>
    <div class="form-group">
      <label>Custom Dropdown (Fixed):</label>
      <div class="custom-dropdown" id="customDropdown">
        <button class="dropdown-button" id="dropdownButton" type="button">
          <span id="dropdownText">Select an option...</span>
          <span class="dropdown-arrow"></span>
        </button>
        <div class="dropdown-options" id="dropdownOptions">
          <div class="dropdown-option" data-value="">Select an option...</div>
          <div class="dropdown-option" data-value="1">Option 1</div>
          <div class="dropdown-option" data-value="2">Option 2</div>
          <div class="dropdown-option" data-value="3">Option 3</div>
        </div>
      </div>
    </div>
    <input id="testInput" type="text" placeholder="Test input">
  </div>
  <script>
    function addLog(msg) { console.log(msg); }

    class CustomDropdown {
      constructor(element) {
        this.element = element;
        this.button = element.querySelector('.dropdown-button');
        this.options = element.querySelector('.dropdown-options');
        this.text = element.querySelector('#dropdownText');
        this.isOpen = false;
        this.selectedValue = '';
        this.init();
      }

      init() {
        this.button.addEventListener('click', (e) => {
          e.preventDefault();
          e.stopPropagation();
          this.toggle();
        });

        document.addEventListener('click', (e) => {
          if (!this.element.contains(e.target)) {
            this.close();
          }
        });

        this.options.addEventListener('click', (e) => {
          if (e.target.classList.contains('dropdown-option')) {
            this.selectOption(e.target);
          }
        });
      }

      toggle() {
        this.isOpen ? this.close() : this.open();
      }

      open() {
        this.element.classList.add('dropdown-open');
        this.isOpen = true;
      }

      close() {
        this.element.classList.remove('dropdown-open');
        this.isOpen = false;
      }

      selectOption(option) {
        const value = option.getAttribute('data-value');
        const text = option.textContent.trim();
        this.selectedValue = value;
        this.text.textContent = text || 'Select an option...';
        this.options.querySelectorAll('.dropdown-option').forEach(opt => opt.classList.remove('selected'));
        option.classList.add('selected');
        this.close();
      }

      setValue(value) {
        const opt = this.options.querySelector(`[data-value="${value}"]`);
        if (opt) this.selectOption(opt);
      }

      getValue() {
        return this.selectedValue;
      }
    }

    document.addEventListener('DOMContentLoaded', () => {
      const dropdown = new CustomDropdown(document.getElementById('customDropdown'));
    });
  </script>
</body>
</html>
]]

function init()
  webViewWindow = g_ui.displayUI('webviewdemo')
  webViewWindow:hide()
  webView = webViewWindow:getChildById('demoWebView')
  if webView then
    webView:loadHtml(html)
  end
  webViewButton = modules.client_topmenu.addLeftButton('webViewDemoButton', tr('WebView Demo'), '/images/topbuttons/terminal', toggle)
end

function terminate()
  webViewWindow:destroy()
  webViewButton:destroy()
end

function toggle()
  if webViewWindow:isVisible() then
    webViewWindow:hide()
  else
    webViewWindow:show()
    webViewWindow:raise()
    webViewWindow:focus()
  end
end