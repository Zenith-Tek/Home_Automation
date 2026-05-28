import os
import time
import requests
import shutil

# --- CONFIGURATION ---
SUPABASE_URL = "https://hbpwmqxwnnffgnncbgeq.supabase.co"
# Use your Service Role Key or Anon Key (Service Role is better for backend scripts)
SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImhicHdtcXh3bm5mZmdubmNiZ2VxIiwicm9sZSI6InNlcnZpY2Vfcm9sZSIsImlhdCI6MTc3OTIwMTQ0MSwiZXhwIjoyMDk0Nzc3NDQxfQ.1XYjqZ6ONMhwuX8IZag_aA02dXb_6BMYjynSmV_-OQo"
STORAGE_BUCKET = "firmware"
TABLE_NAME = "system_control"
SOURCE_BIN = "build/home_automation.bin" # Path to your compiled bin

def create_release():
    print("--- ESP32 Supabase Auto-Release Tool ---")

    # 1. Check if bin file exists
    if not os.path.exists(SOURCE_BIN):
        print(f"Error: {SOURCE_BIN} not found! Did you run 'idf.py build'?")
        return

    # 2. Get Version Info from User
    version = input("Enter new version number (e.g., 1.0.5): ").strip()
    description = input("Enter update description: ").strip()

    # Create a safe filename (replace dots with underscores for the local copy)
    clean_version = version.replace(".", "_")
    new_filename = f"home_automation_v{clean_version}.bin"
    
    # 3. Create a local copy in a 'releases' folder
    if not os.path.exists("releases"):
        os.makedirs("releases")
    
    release_path = os.path.join("releases", new_filename)
    shutil.copy(SOURCE_BIN, release_path)
    print(f"Created local copy: {release_path}")

    # 4. Upload to Supabase Storage
    print(f"Uploading {new_filename} to Supabase Storage...")
    
    upload_url = f"{SUPABASE_URL}/storage/v1/object/{STORAGE_BUCKET}/{new_filename}"
    
    headers = {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
        "Content-Type": "application/octet-stream"
    }

    with open(release_path, 'rb') as f:
        file_data = f.read()
    print(f"  File size: {len(file_data)} bytes")

    # Upload with upsert header so it works whether file exists or not
    headers["x-upsert"] = "true"
    uploaded = False
    for attempt in range(3):
        try:
            print(f"  Attempt {attempt+1}/3...")
            response = requests.post(upload_url, headers=headers, data=file_data, timeout=(30, 300))
            if response.status_code == 200:
                print("Upload Successful!")
                uploaded = True
                break
            else:
                print(f"  Upload returned {response.status_code}: {response.text}")
                if attempt < 2:
                    print("  Retrying...")
                    time.sleep(2)
        except (requests.exceptions.ConnectionError,
                requests.exceptions.Timeout,
                requests.exceptions.SSLError) as e:
            print(f"  Network error: {type(e).__name__}")
            if attempt < 2:
                print("  Retrying in 3 seconds...")
                time.sleep(3)
            else:
                print("  All retries failed. Check your network connection.")
                return
    if not uploaded:
        print("Upload failed after 3 attempts.")
        return

    # 5. Construct the Public URL
    # Format: https://[project-id].supabase.co/storage/v1/object/public/[bucket]/[filename]
    public_url = f"{SUPABASE_URL}/storage/v1/object/public/{STORAGE_BUCKET}/{new_filename}"

    # 6. Update system_control table
    print(f"Updating '{TABLE_NAME}' table to trigger OTA...")
    
    db_url = f"{SUPABASE_URL}/rest/v1/{TABLE_NAME}?id=eq.1" # Updating row with ID 1
    
    db_headers = {
        "apikey": SUPABASE_KEY,
        "Authorization": f"Bearer {SUPABASE_KEY}",
        "Content-Type": "application/json",
        "Prefer": "return=minimal"
    }

    payload = {
        "version": version,
        "bin_url": public_url,
        "update_description": description
    }

    # Use PATCH to update the existing master row
    db_response = requests.patch(db_url, headers=db_headers, json=payload)

    if db_response.status_code in [200, 201, 204]:
        print("--------------------------------------------------")
        print(f"RELEASE {version} IS LIVE!")
        print(f"URL: {public_url}")
        print("Your ESP32s will now detect the update via Realtime.")
        print("--------------------------------------------------")
    else:
        print(f"Database update failed! {db_response.status_code}: {db_response.text}")

if __name__ == "__main__":
    create_release()