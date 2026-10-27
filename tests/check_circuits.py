"""Check the shipped SD database and firmware seed are identical."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[1]
text = (root / 'sdcard/circuits.json').read_text()
seed = (root / 'src/laptiming/DefaultCircuits.h').read_text().split('R"JSON(', 1)[1].rsplit(')JSON"', 1)[0]
assert json.loads(seed) == json.loads(text)
database = json.loads(text)
assert database['schemaVersion'] == 1
cadwell = database['circuits'][0]
assert cadwell['startFinish']['point1'] == {'latitude': 53.310286, 'longitude': -0.059360}
assert cadwell['startFinish']['point2'] == {'latitude': 53.310344, 'longitude': -0.059583}
print('Circuit seed checks passed')
