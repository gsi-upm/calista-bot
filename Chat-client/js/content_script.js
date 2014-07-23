jQuery(document).ready(function($){

    // Creates (or loads from cookie) a random user id for the bot
    username = "";
    if(document.cookie && document.cookie.indexOf("botUser") != -1) {
       data = document.cookie.split(";");
       for (var i = 0; i < data.length; i++) {
           cookie = data[i].trim();
           //console.log(document.cookie);
           if (cookie.indexOf("botUser") == 0) {
               username = cookie.substr("botUser".length, cookie.length -1);
	   }
       }
       // If I couldn't get the user, I set a random one
       //if (username =="") username = randomString(20);
    } else {
       username = randomString(20);
       document.cookie = "botUser="+username+";";
    };
    document.forms["userinput"].elements["user"].value = username;


    /** Hide/show effect */
    $('#bot_chat_window #upper_bar a').click(function(){
        $('#bot_chat_window').toggleClass('visible');
        $('#bot_chat_window #screen_wrapper form input[name=input]').val('');
        $('#bot_chat_window #screen_wrapper form input[name=q]').focus();
    });

    /**/
    $('form#userinput').submit(function(){

        // serialize the form
        var $form = $(this);
        var $input_field = $form.children('input[name=q]');
        if($input_field.val() == '') {
            return false; // prevent sending data when textbox is empty
        };

        var data = $form.serializeArray();
        data.push({name: $(this).attr('name'), value: $(this).attr('value')});
	console.log($input_field.val());
        $('#screen').append($(constructDialogEntry('me', $input_field.val())));
	$input_field.val('');
        scrollDisplay();

	//jQuery.support.cors = true; // :S
        // send the data
        $.ajax({'url': $form.attr('action'), 'data':data, 'crossDomain':true})
          .success(function (data_resp) {
 
              //$('#screen').append($(constructDialogEntry('me', data_resp.dialog.q)));
              //$input_field.val('');
              //scrollDisplay();
              $('#meta_msgs').html($(constructTypingMsg (data_resp.dialog.bot) ));
              $('#meta_msgs').html('');
              $('#screen').append($(constructDialogEntry(data_resp.dialog.bot, data_resp.dialog.response)));
              // make and place the feedbackform
              if (isFeedbackRequiered (data_resp)) {
                  placeFeedbackForm (data_resp);
              }
              // change avatar
              changeavatar (data_resp.dialog.state);
              // scroll display
              scrollDisplay();
              $input_field.focus();   // set focus on main input
            })
          .error(function(data_resp){
              printerrorMessage("No se ha conseguido completar la petición");
              $input_field.focus();   // set focus on main input
            });

        return false;
    });

    // Define feedback form submit (it seems that appending a form does not 
    // add submit fuctionality))
    $("body").on("click", ".feedback_form button", function(e) {

        // serialize the form
        var $form = $(this).parents("form.feedback_form");
        var $div = $(this).parents("div.feedback_req");
        var data = $form.serializeArray();
        data.push({name: $(this).attr('name'), value: $(this).attr('value')});
        // place spinner
        $div.html('<img src="./images/spgray.gif" />');
        // send the data
        $.get($form.attr('action'), data)
          .success(function(data) {
              $div.html('¡Gracias!')
              $form.delay(1500).fadeOut(800);
            })
          .error(function(data){
              $form.slideUp('fast');
              printerrorMessage("No se ha conseguido completar la petición");
            });
    });


    /* This construct html formatted dialog entry */
    function constructDialogEntry (nick, entry) {
        return '<div class="dia_entry">\
                  <span class="nick">' + nick + ':</span>\
                  <span class="text">' + entry + '</span>\
                </div>'
    };

    /* This construct the html formatted common message "Somebody 
     * is typing" message */
    function constructTypingMsg (nick) {
        return '<div class="meta">' + nick + ' está escribiendo...</div>';
    };

    /* This prints a message in the standard error diaplay */
    function printerrorMessage(msg) {
        $('#msg_dialog').text(msg);
        $('#msg_dialog').fadeIn().delay(3000).fadeOut('slow');
    }

    // This checks if the feedbackis requiered
    function isFeedbackRequiered (json) {
        return json.dialog.full_response &&
               (json.dialog.full_response == true ||
                json.dialog.full_response == 'true');
    }

    // This places the feedback form on to the stage
    function placeFeedbackForm (json) {
        var fb_form = constructFeedbackForm (json);
        $('#screen').append($(fb_form));
    }

    // This constructs feedbackform and returns its html representation
    function constructFeedbackForm (json) {
        var hidden_fields = "";
        for (var key in json.dialog) {
          var obj = json.dialog[key];
          hidden_fields += '<input type="hidden" name="' + key + '" value="' + obj + '" />';
        }
        return '<form class="feedback_form" action="' + $('form#userinput').attr('action') + '" method="get">\
                  <div class="feedback_req"><span>¿Te ha servido la respuesta?</span>\
                  <button class="ok" type="submit" name="feedback" value="1"></button>\
                  <button class="nok" type="submit" name="feedback" value="2"></button>\
                  </div>' + hidden_fields +
                '</form>';
    }

    /* This scrolls the display to the bottom, just to show all the messages.
     * It can be overriden to use a different animation */
    function scrollDisplay () {
        // Nice scroll to last message
        $("#screen").scrollTo('100%', 0);  
    }

    /* This function maps the given mood for the bot with the src path to 
     * the appropiate imagen. If no matching is found a default path is 
     * returned. It calls replaceAvatar function. */
    function changeavatar (mood) {
        var default_src = "images/avatars/duke.png";
        var img_map = {'saludo':'images/avatars/duke_hello.png',
                       'despedida':'images/avatars/duke_hello.png',  
                       'fallo1':'images/avatars/duke_waiting.png',
                       'fallo2':'images/avatars/duke_waiting.png',
                       'enfado':'images/avatars/duke_waiting.png',
                       'espera':'images/avatars/duke_thinking.png',
                       'acierto1':'images/avatars/duke_ok.png',
                       'acierto2':'images/avatars/duke_connect.png',
                       'acierto3':'images/avatars/duke_connect.png',
                       'acierto4':'images/avatars/duke_ok.png'};

        if (img_map[mood] != undefined) {
            replaceAvatar (img_map[mood]);
            return;
        }

        replaceAvatar (default_src);
    }

    /** @deprecated */
    function randomAvatar () {
        var img_set = ['images/avatars/duke_hello.png', 
                       'images/avatars/duke_waiting.png',
                       'images/avatars/duke_connect.png',
                       'images/avatars/duke_thinking.png',
                       'images/avatars/duke_ok.png'];
        // Random image switching
        replaceAvatar (img_set[Math.floor(Math.random()*img_set.length)]);
    }

    /* This replaces the avatar with a new one. This function can be overriden.
     * It is expected that it simpli changes the avatar displayed according to 
     * the src path given (of maybe a more conplex configuration. This is the
     * awesomeness of loose coupling in javascript).*/
    function replaceAvatar (src) {
        $('img#bot_avatar').attr('src', src);
    }

    // Generates a Random string for the user id
    function randomString(length) {
        charSet = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        var result = '';
        for (var i = length; i > 0; --i) result += charSet[Math.round(Math.random() * (charSet.length - 1))];
        return result;
    }

});

