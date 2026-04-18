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