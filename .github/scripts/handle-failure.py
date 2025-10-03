#!/usr/bin/env python3
"""
Handle failure script
Create appropriate error summary for failed sync operations
"""

import os
import sys


def main():
    """Main execution function"""
    # Get config from environment variables
    mirror_url = os.environ.get('MIRROR_URL', 'Unknown')
    mirror_branch = os.environ.get('MIRROR_BRANCH', 'Unknown')
    target_branch = os.environ.get('TARGET_BRANCH', 'Unknown')
    
    print("❌ Mirror sync workflow failed")
    
    summary = f"""## Mirror Sync Failed ❌

The mirror sync workflow failed. Possible causes:

- **Authentication issues**: Check GITLAB_TOKEN_USERNAME and GITLAB_TOKEN_PASSWORD secrets
- **Network issues**: Mirror repository might be unreachable
- **Merge conflicts**: Manual resolution may be required
- **Permission issues**: Check repository permissions

**Configuration:**
- Mirror URL: {mirror_url}
- Mirror Branch: {mirror_branch}
- Target Branch: {target_branch}

Please check the workflow logs for more details.
"""
    
    try:
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as f:
            f.write(summary)
    except Exception as e:
        print(f"❌ Error writing summary: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()