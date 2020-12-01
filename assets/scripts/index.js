var handleClick = function (el) {
  decorateSidebar(el);
  showMainContent(el);
}

var decorateSidebar = function (el) {
  let items = document.getElementsByClassName("sidebar-item");
  for (let i = 0; i < items.length; i++) {
    items[i].classList.remove("sidebar--selected");
  }
  el.parentElement.classList.add("sidebar--selected");
}

var showMainContent = function (el) {
  let items = document.getElementsByClassName("main-item");
  for (let i = 0; i < items.length; i++) {
    items[i].classList.remove("main--selected");
  }
  let divId = "main-" + el.parentElement.id.split("-")[1];
  document.getElementById(divId).classList.add("main--selected");
}