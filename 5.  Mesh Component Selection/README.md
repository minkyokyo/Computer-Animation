# Drag를 이용하여 vertex or face or egde를 선택하기 
![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/abdc4ddf-5b5b-45da-9da3-2172c30d3b57)  
마우스 드래그로 직사각형 안에 포함된 face를 선택한 모습이다.  

# Drag
gluUnProject() 를 이용해 screen space상의 마우스 좌표를 world space상의 마우스 좌표로 옮겼다.  
z값은 늘 제일 맨 앞에 렌더링 되어야 하기 때문에 0을 준다.  
그 상태에서 glLine, glVertex함수 등을 이용해 사각형을 그려준다.  

# Opengl Selection
glSelectBuffer(), gluPickMatrix()등을 통해서 selection되었는지 판단했다.  
