import os
import xml.etree.ElementTree as ET

# Function to list .xml files and ask user to choose one
def choose_xml_file():
    # Get a list of all .xml files in the current directory
    xml_files = [f for f in os.listdir() if f.endswith('.xml')]
    
    # Check if there are any .xml files
    if not xml_files:
        print("No XML files found in the current directory.")
        return None

    # Display files to the user
    print("Available XML files:")
    for index, file in enumerate(xml_files, 1):
        print(f"{index}. {file}")
    
    # Ask the user to choose a file by entering a number
    try:
        choice = int(input("Enter the number of the file you want to process: "))
        if 1 <= choice <= len(xml_files):
            return xml_files[choice - 1]  # Return the selected file
        else:
            print("Invalid choice. Please choose a valid number.")
            return None
    except ValueError:
        print("Invalid input. Please enter a number.")
        return None

# Parse MusicXML and convert it to text
def parse_musicxml(file_path):
    tree = ET.parse(file_path)
    root = tree.getroot()
    namespace = {'ns': 'http://www.musicxml.org/xmlns'}  # Some files might use this
    
    notes_text = []
    tempo = "Unknown"
    time_signature = "Unknown"
    key_signature = "Unknown"
    clef = "Unknown"
    
    accidental_map = {"1": "#", "-1": "b", "0": "♮"}
    key_signature_map = {
        -7: "C♭ Major", -6: "G♭ Major", -5: "D♭ Major", -4: "A♭ Major", -3: "E♭ Major",
        -2: "B♭ Major", -1: "F Major", 0: "C Major", 1: "G Major", 2: "D Major",
        3: "A Major", 4: "E Major", 5: "B Major", 6: "F♯ Major", 7: "C♯ Major"
    }
    
    for part in root.findall('.//part'):
        for measure in part.findall('.//measure'):
            # Get time signature (if present)
            time_elem = measure.find('.//time')
            if time_elem is not None:
                beats = time_elem.find('beats').text
                beat_type = time_elem.find('beat-type').text
                time_signature = f"{beats}/{beat_type}"
            
            # Get tempo (if present)
            sound_elem = measure.find('.//sound')
            if sound_elem is not None and 'tempo' in sound_elem.attrib:
                tempo = sound_elem.attrib['tempo']
            
            # Get key signature (if present)
            key_elem = measure.find('.//key')
            if key_elem is not None:
                fifths = int(key_elem.find('fifths').text)  # Convert fifths to integer
                mode_elem = key_elem.find('mode')
                mode = mode_elem.text if mode_elem is not None else "major"
                key_signature = key_signature_map.get(fifths, "Unknown Key")
            
            # Get clef (if present)
            clef_elem = measure.find('.//clef')
            if clef_elem is not None:
                sign = clef_elem.find('sign').text
                clef = f"{sign} Clef"
            
            measure_notes = []
            for note in measure.findall('.//note'):
                pitch_elem = note.find('pitch')
                duration = note.find('duration').text if note.find('duration') is not None else "Unknown"
                note_type = note.find('type').text if note.find('type') is not None else "Unknown"
                
                if pitch_elem is not None:
                    step = pitch_elem.find('step').text
                    octave = pitch_elem.find('octave').text
                    
                    # Get accidental
                    accidental = ""
                    alter_elem = pitch_elem.find('alter')
                    if alter_elem is not None:
                        accidental = accidental_map.get(alter_elem.text, "")
                    
                    note_repr = f"{step}{accidental}{octave} ({note_type}, {duration})"
                else:
                    note_repr = "Rest"
                
                # Check if it's part of a chord
                if note.find('chord') is not None:
                    note_repr = f"[Chord] {note_repr}"
                
                # Check for lyrics
                lyric_elem = note.find('lyric/text')
                if lyric_elem is not None:
                    note_repr += f" - Lyric: {lyric_elem.text}"
                
                measure_notes.append(note_repr)
            
            notes_text.append(f"Measure {measure.attrib['number']}: " + ", ".join(measure_notes))
    
    output = f"Tempo: {tempo} BPM\nTime Signature: {time_signature}\nKey Signature: {key_signature}\nClef: {clef}\n\n" + "\n".join(notes_text)
    return output

# Main function to run the script
def main():
    xml_file = choose_xml_file()
    if xml_file is None:
        return
    
    output_text = parse_musicxml(xml_file)

    # Get the base file name without extension (for output file name)
    output_filename = os.path.splitext(xml_file)[0] + ".txt"

    # Save to a text file with UTF-8 encoding
    with open(output_filename, "w", encoding="utf-8") as f:
        f.write(output_text)

    print(f"Conversion complete. Check {output_filename}")

if __name__ == "__main__":
    main()