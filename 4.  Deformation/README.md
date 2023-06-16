![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/50348595-a29f-4d12-a43a-d4106a6515e1)

vertex를 twist, wave한 결과이다.

# twist
![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/e3126102-29fa-4bee-800d-80368548e068)
회전 행렬을 적용하여 vertex 위치 변환  
이 작업은 vertex shader에서 진행한다.  
위치 뿐만 아니라, normal vector도 변환한다.  

# wave
![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/50702f21-a48c-44e0-bff4-1e914539dbb7)
wave는 위 아래로만 변화하면 된다. z축의 변화만 신경쓴다.  
변화한 normal vector를 구하기 위해선   
![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/bb801e36-b281-42ee-b68b-41cfa868e316)
자코비안 행렬을 다음과 같이 구하고 곱한다.

