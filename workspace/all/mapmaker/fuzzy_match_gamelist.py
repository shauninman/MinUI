import os
import xml.etree.ElementTree as ET
from fuzzywuzzy import fuzz
from fuzzywuzzy import process

def load_gamelist(file_path):
    tree = ET.parse(file_path)
    root = tree.getroot()
    games = []
    for game in root.findall('game'):
        title = game.find('name').text
        games.append(title)
    return games

def find_best_match(input_title, game_titles):
    best_match = process.extractOne(input_title, game_titles, scorer=fuzz.token_sort_ratio)
    return best_match

def preprocess_filename(filename):
    # Remove extension and normalize filename
    name, _ = os.path.splitext(filename)
    return name.lower()

def process_folder(folder_path, gamelist_path):
    # Load game titles from the XML file
    game_titles = load_gamelist(gamelist_path)
    
    # Open map.txt to write results in the folder being scanned
    map_file_path = os.path.join(folder_path, "map.txt")
    with open(map_file_path, "w") as output_file:
        # Loop through all files in the specified folder
        for root, _, files in os.walk(folder_path):
            for file in files:
                # Ignore .txt and .xml files
                if file.endswith('.txt') or file.endswith('.xml'):
                    continue
                
                # Preprocess the filename
                preprocessed_filename = preprocess_filename(file)
                
                # Find the best match
                best_match = find_best_match(preprocessed_filename, game_titles)
                
                # Write the result to map.txt
                output_file.write(f"{file}\t{best_match[0]}\n")

def main():
    # Read the folders.xml file and process each folder
    tree = ET.parse("folders.xml")
    root = tree.getroot()
    for folder in root.findall('folder'):
        folder_path = folder.find('path').text
        gamelist_path = folder.find('gamelist').text
        process_folder(folder_path, gamelist_path)

if __name__ == "__main__":
    main()
