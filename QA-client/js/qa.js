// Autocomplete data

var autocom_questions = [
    "Que es un for?",
    "Que es un while?",
    "Como hago una interfaz?",
    "Que es javadoc?",
    "Donde puedo ver un ejemplo de for?",
    "Que es una excepción?",
    "Querría un ejemplo de while",
    "Como hago un bucle"
];

jQuery(document).ready(function($){

    /* Store the url shown */
    var current_url_shown = "" // this is used to avoid continous recharging of the web
    
    /* Vademecum base url */
    var vademecum_base = "http://www.dit.upm.es/~pepe/libros/vademecum/index.html?n="

    
    // Creates (or loads from cookie) a random user id for the bot
    var username = "";
    if(document.cookie && document.cookie.indexOf("botUser") != -1) {
       data = document.cookie.split(";");
       for (var i = 0; i < data.length; i++) {
           cookie = data[i].trim();
           //console.log(document.cookie);
           if (cookie.indexOf("botUser") == 0) {
               username = cookie.substr("botUser=".length, cookie.length -1);
       }
       }
       // If I couldn't get the user, I set a random one
       //if (username =="") username = randomString(20);
    } else {
       username = randomString(5);
       document.cookie = "botUser="+username+";";
    };
    // Set the username in the span    
    $('#username').html(username);

    /** Hide/show effect */
    $('#bot_chat_window #upper_bar a').click(function(){
        $('#bot_chat_window').toggleClass('visible');
        $('#bot_chat_window #screen_wrapper form input[name=question]').val('');
        $('#bot_chat_window #screen_wrapper form input[name=question]').focus();
    });
    
    // Autocomplete
    //$('#question').autocomplete({source: autocom_questions});
    /**/
    $('#qa-form').on('submit', function(){

        // serialize the form
        var form = $(this);
        var form_data = form.serializeArray();
        var input_field = form.children('input[name=question]');
        if (form[0].value == '') {
            //TODO: Add an alert to say there needs to be data
            return false;
        };
    
        var json_data = {};
        $.map(form_data, function(n, i){
            json_data[n['name']] = n['value'];
        });
        json_data.username = username;

        $('#screen').append($(constructDialogEntry('me', json_data.question)));
        input_field.val('');
        scrollDisplay();
        //var data = JSON.stringify(json_data);

        jQuery.support.cors = true; // :S
        // send the data
        
        $.ajax({url: form.attr('action'), 
            data: json_data,
            dataType: 'json',
            contentType: 'application/json;charset=UTF-8',
            success: function (data_resp) {
                console.log(data_resp);
                // Set the url as a response
                if (data_resp.resource) {
                    if(data_resp.resource != current_url_shown) {
                        // I want the filename from that url:
                        var name_start = data_resp.resource.lastIndexOf('/')+1
                        var filename = data_resp.resource.substring(name_start)
                        $('#iframe-qa').attr('src', vademecum_base + filename);
                        current_url_shown = vademecum_base + filename;
                        // Add a generic response
                        $('#screen').append(
                            $(constructDialogEntry('Dent',
                                                   "Te muestro la información que tengo sobre "
                                                  + data_resp.title)));
                        scrollDisplay();
                        $('#iframe-qa').show();
                    }
                    /*related_list="<ul>";
                    data_resp.links.forEach(function(entry){
                        related_list+='<li>'+entry.title+'</a>';
                    });
                    related_list+='</ul>';
                    $('#related-list').html(related_list);
                    $('#related-container').fadeIn('slow');*/
                } else {
                    $('#screen').append(
                    $(constructDialogEntry('Dent',
                                            data_resp.error)));
                    scrollDisplay();
                    
                }
            },
            error: function(data_resp){
                console.log("Error connecting to the contoller");
                return false;
            }
        });

        return false;
    });
    

    // Generates a Random string for the user id
    function randomString(length) {
        charSet = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        var result = '';
        for (var i = length; i > 0; --i) result += charSet[Math.round(Math.random() * (charSet.length - 1))];
        return result;
    }
    
        /* This scrolls the display to the bottom, just to show all the messages.
     * It can be overriden to use a different animation */
    function scrollDisplay () {
        // Nice scroll to last message
        $("#screen").scrollTo('100%', 0);  
    }
    
    /* This construct html formatted dialog entry */
    function constructDialogEntry (nick, entry) {
        return '<div class="dia_entry">\
                  <span class="nick">' + nick + ':</span>\
                  <span class="text">' + entry + '</span>\
                </div>'
    };

});

