jQuery(document).ready(function($){
   
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
              
        $('#chat-responses').append('<p class="bot-r"><b>Dent:</b><br/>'+
                                 'Hola! ¿En qué puedo ayudarte?</p>');
       
    } else {
       username = randomString(5);
       document.cookie = "botUser="+username+";";
       // First time answer, send a question to get the gambit response
       json_data = {'username': username, 'question':'Hola'};
       $.ajax({ url: $('#ask-form').attr('action'),
                 data: json_data,
                 dataType: 'json',
                 contentType: 'application/json;charset=UTF-8',
                 success: populateForm,
                 error: function(data_resp) {
                     alert("Esto es un error");
                    console.log("Error connection to the controller");
                 }
       });
    };
    // Set the username in the span    
    $('#username').html(username);
    
    $('#ask-form').on('submit', function() {
        var form = $(this);
                var form_data = form.serializeArray();
        if (form[0].value == '') {
            //TODO: Add an alert to say there needs to be data
            return false;
        };
        
        var json_data = {};
        $.map(form_data, function(n, i){
            json_data[n['name']] = n['value'];
        });
        
        json_data.username = username;
        
        // Add the question 
        $('#chat-responses').append('<p class="user-q"><b>Me:</b><br/>'
                                +json_data.question+'</p>');
        $('#question').val('')
        $.ajax({ url: form.attr('action'),
                 data: json_data,
                 dataType: 'json',
                 contentType: 'application/json;charset=UTF-8',
                 success: populateForm,
                 error: function(data_resp) {
                     alert("Esto es un error");
                    console.log("Error connection to the controller");

                 }
        });
        return false;
    });
    
    function populateForm (data_resp) {
        if (data_resp.answer) { 
            data_resp.answer.forEach(function(answer) {
                $('#chat-responses').append('<p class="bot-r"><b>Dent:</b><br/>'
                                            +answer+'</p>');
                });
                $('#iframe-qa').attr('src', data_resp.resource);   
        }else{
            $('#chat-responses').append('<p class="bot-r"><b>Dent:</b><br/>'+
                                'Lo siento, no puedo responder a esa pregunta</p>');
            }
    }
    // Generates a Random string for the user id
    function randomString(length) {
        charSet = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        var result = '';
        for (var i = length; i > 0; --i) result += charSet[Math.round(Math.random() * (charSet.length - 1))];
        return result;
    }
    
});
