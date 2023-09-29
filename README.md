# AutoBarrier_LPC1769

La idea a desarrollar como prototipo es una “Barrera para autos” con sensor ultrasónico que mide la presencia de un auto que esté ingresando a un lugar. 
La persona que se encuentre intentando pasar dicha barrera debe ingresar un código determinado que se guarda en un espacio de memoria, como forma de 
registro.

Si un administrador quiere revisar ese registro puede ingresar otro código y el mismo se transmitirá por comunicación serie a una computadora.
Además, se podrán registrar usuarios desde el teclado. Cuando el sensor detecta que un auto ya cruzó la barrera, la cierra.

![image](https://github.com/generobruno/AutoBarrier_LPC1769/assets/36767810/5130ef19-194b-4283-89f7-600118928efb)

Funciones de usuario:
<ul>
  <li>Permitir el acceso solo al personal con código autorizado.
  <li>Registrar el acceso del mismo.
  <li>Bajar automáticamente la barrera luego de que el personal haya ingresado.
  <li>Transmitir el registro por medio de comunicación serie.
</ul>

El procedimiento con el que se utilizará el prototipo es el siguiente:

Primeramente, un administrador debe configurar una contraseña pulsando la tecla A y luego la combinación de números que será dicha contraseña.
Luego, para registrar a un empleado, el administrador deberá presionar la tecla B seguida de la contraseña y luego el número con el que se registra a 
dicho empleado.

Cuando un empleado se acerque a la barrera deberá ingresar su número asignado, si es correcto la barrera se abrirá y se registrará en memoria el ingreso 
de dicho empleado.

Al cerrarse la barrera, se revisará que la misma tenga batería suficiente. En caso contrario, se mostrará un mensaje de batería baja, prendiendo un led.
Luego de que el empleado haya ingresado, un sensor detectará la ausencia del vehículo y cerrará automáticamente la barrera.

En caso de querer obtener el registro de los empleados que hayan ingresado, el administrador deberá conectar su computadora, presionar la tecla C, 
seguido de la contraseña y la información se transmitirá a esta para poder revisar dichos datos.

El código de un empleado consta de 4 números, el primer dígito hace referencia al área donde trabaja, y el resto es el identificador personal del 
trabajador. De esta manera, cuando se transmita esta información a la computadora, podremos comparar estos datos con alguna base de datos donde se
encuentren los nombres de los empleados.

