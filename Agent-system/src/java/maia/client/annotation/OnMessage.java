/*
 * Copyright 2013 miguel Grupo de Sistemas Inteligentes (GSI UPM) 
 *                                         <http://gsi.dit.upm.es>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
package maia.client.annotation;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * <p>It is discouraged to assign multiple <code>onMessage</code> annotations 
 * to the same target.</p>
 * 
 * 
 * <b>Project:</b> maia-client<br />
 * <b>Package:</b> maia.client<br />
 * <b>Class:</b> OnMessage.java<br />
 * <br />
 * <i>GSI 2013</i><br />
 *
 * @author Miguel Coronado (miguelcb@gsi.dit.upm.es)
 * @version	Jun 4, 2013
 *
 */
@Target(value = ElementType.METHOD)
@Retention(value = RetentionPolicy.RUNTIME)
public @interface OnMessage {
    
    String value();    
    
}
