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
       // If I couldn't get the user, I set a random one
       //if (username =="") username = randomString(20);
    } else {
       username = randomString(5);
       document.cookie = "botUser="+username+";";
    };
    // Set the username in the span    
    $('#username').html(username);
    
    $('#ask-form').on('submit', function() {
        var form = $(this);
        
           var json_data = {};
        $.map(form_data, function(n, i){
            json_data[n['name']] = n['value'];
        });
        
        
        $.ajax({ url: form.attr('action'),
                 data: json_data,
                 dataType: 'json',
                 contentType: 'application/json;charset=UTF-8',
                 success: function (data_resp) {
                    
                 },
                 error: function(data_resp) {
                    console.log("Error connection to the controller");
                 }
        });
    });
    
    // Generates a Random string for the user id
    function randomString(length) {
        charSet = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        var result = '';
        for (var i = length; i > 0; --i) result += charSet[Math.round(Math.random() * (charSet.length - 1))];
        return result;
    }
    
});