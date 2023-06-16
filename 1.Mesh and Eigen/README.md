# mesh와 shrunken face를동시에 그리기
![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/8d426567-e8c9-4820-b6bd-65256bad8243)  

# shrunken face
![image](https://github.com/minkyokyo/Computer-Animation/assets/71928522/884a535f-f8b0-4ba7-8c6a-684c8f545834)
shrunken face를 만들기 위해서, Vertex들의 중심점을 알아야 한다.
Shrink vertices to COM 식에서 알파는 수축되는 정도이다. 0.1을 주면 10퍼센트 수축한다.  

# offset
glPolygonOffset() 함수를 사용  
객체들의 깊이값( z-buffer)들이 같을 때, 겹치는 현상이 발생할 수 있는데, 이 때에 offset을 주어서 겹치지 않게 render 할 수 있다.

# 기타
 mesh cpp, mesh.h -> .off파일에서 vertex, egde, vertex index를 읽는다.
 glSetup.cpp, glSetup.h -> openGL 기본 셋팅
