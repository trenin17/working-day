import os

exclude = ["third_party", ".github", ".pgdata", ".vscode", "aws", "build", "build_release", "build_debug", ".git", ".cores", "__pycache__", ".pytest_cache", "templates"]

lc = {}
endings = {}
total = 0

for root, dirs, files in os.walk(".", topdown=True):
    dirs[:] = [d for d in dirs if d not in exclude]
    path = root.split(os.sep)
    #print((len(path) - 1) * '---', os.path.basename(root))
    for file in files:
        ending = file[file.find('.')+1:]
        num = 0
        print(file)
        with open(os.path.join(root, file), 'r') as fp:
            num = len(fp.readlines())

        #print(num)

        if ending not in lc:
            lc[ending] = num
        else:
            lc[ending] += num

        total += num

        if ending not in endings:
            endings[ending] = [file]
        else:
            endings[ending].append(file)
        
print(total)
for c in lc:
    print(c, lc[c])