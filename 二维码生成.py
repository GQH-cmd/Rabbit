import qrcode
import json
import os

os.makedirs("qrcodes", exist_ok=True)

for farm in range(1, 2):
    for house in range(1, 3):
        for row in range(1, 4):
            for col in range(1, 4):
                for layer in range(1, 3):

                    data = {
                        "farm": farm,
                        "house": house,
                        "row": row,
                        "col": col,
                        "layer": layer
                    }

                    data_str = json.dumps(data)

                    filename = f"F{farm}_H{house}_R{row}_C{col}_L{layer}.png"

                    img = qrcode.make(data_str)
                    img.save(f"qrcodes/{filename}")