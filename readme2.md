Comandos enviados por telegram

encender: Desbloquea el sistema y permite que las lámparas funcionen. Borra alertas de temperatura.

apagar: Manual de Paro de Emergencia. Apaga todas las lámparas y bloquea el sistema hasta que envías "encender".

estatus: Solicita un reporte inmediato del estado actual (temperatura, lámparas, tiempo, etc).

setpoint <##>:Cambia la temperatura objetivo.
	ej: setpoint 110

set alert <##>: Cambia el límite de seguridad donde salta la alarma crítica.
	ej: set alert 122 (Si llega a 122°C, se apaga todo)

count down <##>: Inicia una cuenta regresiva en minutos . Al terminar, el horno se apaga y bloquea.
	ej: count down 1 (se paga en 1 minuto)

count down disabled: Cancele el temporizador inmediatamente y deje el horno funcionando normalmente.

spam on: Activa el envío automático del estatus periódicamente.

spam off: Desactiva los reportes automáticos (solo hablará si le preguntas o hay alerta).

freq spam <##>: Define cada cuántos minutos quieres recibir el informe automático.
	ej: freq spam 10 (se aparece cada 10 minutos)
