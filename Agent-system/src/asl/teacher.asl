
/* Initial beliefs and rules */

//Rule to test if the asked concept Z is much more complex than the previously asked concept X
//Z: asked  X: previous  Y: iterator
more_complex(Z,X) :-   
   requires(Z,X) | (requires(Z,Y) & more_complex(Y,X)).
   

//Learning path establishing requirements between units
requires(concept_iteracion,concept_bucles). 
requires(concept_for,concept_iteracion).  //con la regla se sobreentiende que dowhile requiere bucles
requires(concept_while,concept_iteracion).
requires(concept_dowhile,concept_while). //con la regla se sobreentiende que dowhile requiere bucles
requires(concept_break,concept_iteracion).
requires(concept_continue,concept_iteracion).
   


/* Initial goals */

!launch.

/* Plans */

+!launch
	<- 
	
	maia.start. //Starts the Maia connection for listening incomming messages
	
	
	
	
-!launch(x) 
 	<- .print("Shutting down...").


//When the concept is explained, note it and reminds that it is possible to ask for examples
+explained(C)[user(U)]
	: numexplained(N)[user(U)] & N<=1 
	<- 
		-+numexplained(N+1)[user(U)];
		+explainedlist(C,N)[user(U)];
		
		
		!check_require(C,N);
		
		maia.send("[sendcs (java offer example)] [user ",U,"]").
	
//When the concept is explained, note it and send a test for previously explained concepts
+explained(C)[user(U)] 
	: numexplained(N)[user(U)] & N>1
	<- 
	   -+numexplained(N+1)[user(U)];
	   +explainedlist(C,N)[user(U)];
	  
	  
	  !check_require(C,N);
	  
	  ?explainedlist(A,N-2)[user(U)];
	  
	  -+test_answer(A)[user(U)];
	  
	  maia.send("[sendcs (java test ",A,")] [user ",U,"]").
	  
//When new user, sets numexplained(0) and refreshes fact to trigger one of the previous plans
+explained(C)[user(U)]
	<- 	+numexplained(0)[user(U)];
		-+explained(C)[user(U)].
	  	  
//Checks if the user answer is the correct one for the previous test sent (case of correct answer)	  
+affirm(R)[user(U)] 
	: test_answer(A)[user(U)] & R=A
	<- 	maia.send("[sendcs (java testok ",A,")] [user ",U,"]").
	  
//Checks if the user answer is the correct one for the previous test sent (case of incorrect answer)  	  	   
+affirm(R)[user(U)]
	: test_answer(A)[user(U)] & R\==A
	<- 	maia.send("[sendcs (java testfail ",A,")] [user ",U,"]").
	


//Checks if the explained concept (C) is much more complex than a previously explained one (L)
+!check_require(C,M) : explainedlist(L,M-1)[user(U)] & more_complex(C,L) & not requires(C,L)
    <- 
    	?requires(N,L);
       maia.send("[sendcs (java recommend ",N,")] [user ",U,"]").
	   

	
-!check_require(C,N)  
 	<- .print("Ok"). 
	

//Recommends a random concept if the user is returning (not the first time using the system)
+returning[user(U)]
	<- 	maia.send("[sendcs (java recommend random)] [user ",U,"]").



	
		   