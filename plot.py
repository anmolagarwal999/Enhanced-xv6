
  
# Opening file 
file1 = open('a.txt', 'r') 
count = 0

x=[]
y=[]
colors=[]

# Using for loop 
print("Using for loop") 
for line in file1: 
    tmp=line.strip()
    # print(tmp)
    if len(tmp)>0:
        if tmp[0]=='[':
            result = [x.strip() for x in tmp.split(' ')]
            # print(result)
            if len(result)==4:
                time_str=result[0]
                colors.append(int(result[1]))
                y.append(int(result[2]))
                tmp_str=time_str[1:-1]
                x.append(int(tmp_str))
                # print(tmp_str)



print(x)
print(y)
print(colors)
# Closing files 
file1.close() 


import pandas as pd
from matplotlib import pyplot as plt

plt.style.use('seaborn')


# plt.scatter(view_count, likes, c=ratio, cmap='summer',
#             edgecolor='black', linewidth=1, alpha=0.75)

# cbar = plt.colorbar()
# cbar.set_label('Like/Dislike Ratio')

# plt.xscale('log')
# plt.yscale('log')

# plt.title('Trending YouTube Videos')
# plt.xlabel('View Count')
# plt.ylabel('Total Likes')

# plt.tight_layout()

# plt.show()