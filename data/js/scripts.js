$(document).ready(function () {
  "use strict";
  var statusUrl = './msg/status';
  var sendUrl = './msg/send?msg=';
  var closeIconUrl = './img/close.svg';
  var $sendButton = $('#send-button');
  var $errors = $('#errors');
  var $messageInput = $('#message');
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
    $.ajax(sendUrl + paramValue, { cache: false })
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

  function dismissError() {
    $(this.parentElement).remove();
  }

  function addErrorMessage(msg) {
    var $closeIcon = $('<img/>')
      .attr('src', 'img/close.svg')
      .addClass('error-icon')
      .on('click', dismissError);
    var newError = $('<div/>').addClass("error").append(msg).append($closeIcon);

    $errors.append(newError);
    console.log(msg);
  }

  function setStatus(status) {
    if (status === 'READY') {
      $messageInput.addClass('ready');
      $messageInput.removeClass('busy');
    } else if (status === 'BUSY') {
      $messageInput.removeClass('ready');
      $messageInput.addClass('busy');
    } else {
      $messageInput.removeClass('ready');
      $messageInput.removeClass('busy');
    }
  }

  (function updateStatus() {
    $.ajax(statusUrl, { cache: false })
      .done(function (data) {
        setStatus(data);
        if (data !== 'READY' && data != 'BUSY') {
          if (!statusErrorReported) {
            addErrorMessage("Error for status check request.");
            statusErrorReported = true;
          }
        }
      })
      .fail(function (data) {
        if (!statusErrorReported) {
          addErrorMessage("Error for status check request.");
          statusErrorReported = true;
        }
      })
    setTimeout(updateStatus, 2500);
  })();

});
