$(document).ready(function () {
  "use strict";
  var sendUrl = './msg/send?msg=';
  var closeIconUrl = './img/close.svg';
  var $sendButton = $('#send-button');
  var $errors = $('#errors');
  var $messageInput = $('#message');
  var $wpmInput = $('#wpm');
  var statusErrorReported = false;

  (function downloadAndCacheCloseIcon() {
    $.ajax(closeIconUrl, { cache: true })
      .fail(function (data) {
        addErrorMessage("Error while getting icons.");
      })
  })();

  $sendButton.on('click', function () {
    setButtonDisabled(true);
    var msg = $messageInput.val();
    var paramValue = encodeURIComponent(msg);
    var url = sendUrl + paramValue;
    var wpm = $wpmInput.val();
    if (wpm) {
      url += "&wpm=" + wpm;
    }
    $.ajax(url, { cache: false })
      .fail(function (data) {
        addErrorMessage(data.responseText || data.statusText + ": " + data.state());
      })
      .always(function () {
        setButtonDisabled(false);
      });
  });

  function setButtonDisabled(disabled) {
    $sendButton.toggleClass('disabled', disabled);
  }

  function addErrorMessage(msg) {
    var $closeIcon = $('<img/>')
      .attr('src', 'img/close.svg')
      .addClass('error-icon')
      .on('click', dismissError);
    var newError = $('<div/>').addClass("error").append(msg).append($closeIcon);

    $errors.prepend(newError);
    console.log(msg);
  }

  function dismissError() {
    $(this.parentElement).remove();
  }
});
