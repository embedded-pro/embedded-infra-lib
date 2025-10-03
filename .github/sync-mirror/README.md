# Mirror Repository Sync

This directory contains a GitHub Action and workflow for automatically syncing changes from a GitLab mirror repository and creating pull requests with the changes.

## Overview

The sync system consists of:
- **Action** (`.github/sync-mirror/action.yml`): Reusable action that handles the sync logic including activity checking
- **Workflow** (`.github/workflows/sync-mirror.yml`): Automated workflow that runs the sync action monthly

## Setup

### 1. Configure Repository Secrets

For the private GitLab mirror repository, you'll need to set up authentication secrets:

1. Go to your repository **Settings** → **Secrets and variables** → **Actions**
2. Add the following repository secrets:

| Secret Name | Description | Example |
|-------------|-------------|---------|
| `GITLAB_TOKEN_USERNAME` | Username for GitLab repository | `your-gitlab-username` |
| `GITLAB_TOKEN_PASSWORD` | GitLab Personal Access Token | `glpat-xxxxxxxxxxxx` |

### 2. GitLab Personal Access Token Setup

1. Go to GitLab → **User Settings** → **Access Tokens**
2. Create a new token with the following scopes:
   - `read_repository` (to access the mirror repository)
3. Copy the token and add it as `GITLAB_TOKEN_PASSWORD` secret

### 3. Mirror Repository Configuration

The workflow is configured to sync from:
- **Repository**: `https://gitlab.com/embedded-library/mirrors/amp-embedded-infra-lib.git`
- **Branch**: `main`

This configuration is hardcoded in the workflow for security and consistency.

## Usage

### Automatic Sync (Scheduled)

The workflow runs automatically:
- **Monthly on the 1st** at 2 AM UTC via cron schedule
- Only syncs if there are commits in the last month (efficiency optimization)
- Activity check prevents unnecessary workflow runs when the mirror hasn't changed

### Manual Sync

You can trigger the sync manually:

1. Go to **Actions** → **Sync Mirror Repository**
2. Click **Run workflow**
3. Optionally override:
   - Target branch (default: `main`)
   - Force sync (ignore recent activity check)

### Using the Action in Other Workflows

You can also use the sync action in your own workflows:

```yaml
jobs:
  custom-sync:
    runs-on: ubuntu-latest
    steps:
      - uses: ./.github/sync-mirror
        with:
          mirror-url: 'https://gitlab.com/your-org/mirror-repo.git'
          mirror-branch: 'develop'
          target-branch: 'main'
          github-token: ${{ secrets.GITHUB_TOKEN }}
          mirror-username: ${{ secrets.GITLAB_TOKEN_USERNAME }}
          mirror-token: ${{ secrets.GITLAB_TOKEN_PASSWORD }}
          check-activity: 'true'
          activity-period: '1 week ago'
          force-sync: 'false'
```

## How It Works

1. **Activity Check**: (Optional) Checks if there are recent commits in the mirror repository
2. **Fetch**: Adds the mirror repository as a remote and fetches the specified branch
3. **Merge**: Creates a new branch and attempts to merge changes from the mirror
4. **Detect Changes**: Compares the sync branch with the target branch
5. **Create PR**: If changes exist, creates a pull request with detailed information
6. **Cleanup**: Removes temporary branches if no changes are detected

## Troubleshooting

### Authentication Issues

**Problem**: `fatal: Authentication failed`

**Solutions**:
- Verify `GITLAB_TOKEN_USERNAME` and `GITLAB_TOKEN_PASSWORD` secrets are correctly set
- For GitLab: Use a Personal Access Token with `read_repository` scope
- Ensure the token has access to the specific mirror repository
- Check that the username matches your GitLab username exactly

### Merge Conflicts

**Problem**: Automatic merge fails due to conflicts

**Solutions**:
- The workflow will fail and report conflicts
- Manually resolve conflicts:
  ```bash
  git clone <your-repo>
  git remote add mirror <mirror-url>
  git fetch mirror
  git checkout -b manual-sync
  git merge mirror/<branch>
  # Resolve conflicts manually
  git commit
  git push origin manual-sync
  # Create PR manually
  ```

### No Changes Detected

**Problem**: Workflow runs but says "No changes detected"

**Possible Causes**:
- Mirror repository is already in sync
- Mirror branch hasn't changed since last sync
- Target branch already contains all mirror changes

### Workflow Skipped

**Problem**: Scheduled workflow says "Sync skipped"

**Possible Causes**:
- No commits in mirror repository in the last month (expected behavior for monthly sync)
- Repository is inactive (GitHub may disable workflows)
- Use manual trigger with "force-sync" to override activity check

### Activity Check Issues

**Problem**: Sync always skipped even when there are recent commits

**Solutions**:
- Check the `activity-period` setting (default: "1 month ago" for scheduled runs)
- Use manual trigger with `force-sync: true` to bypass activity check
- Verify the mirror repository branch has the expected commits

## Security Considerations

- **Secrets**: Never commit authentication tokens to the repository
- **Permissions**: The workflow has minimal required permissions (`contents: write`, `pull-requests: write`)
- **Review**: Always review automatically created PRs before merging
- **Branch Protection**: Consider requiring reviews for PRs targeting protected branches

## Customization

### Workflow Schedule

Change the sync frequency by modifying the cron expression:

```yaml
schedule:
  - cron: '0 6 * * 1'  # Weekly on Mondays at 6 AM UTC
  - cron: '0 2 15 * *' # Mid-month on the 15th at 2 AM UTC
```

### Activity Check Period

Customize how far back to check for activity:

```yaml
# In the workflow, update the action inputs:
activity-period: '2 weeks ago'  # Check last 2 weeks
activity-period: '3 months ago' # Check last 3 months
```

### PR Template

The PR title and body are now configured as defaults in the action.yml file. To customize them, you can override the inputs when calling the action.

## Monitoring

The workflow provides detailed step summaries and logs for monitoring:
- Check **Actions** tab for workflow run history
- Review step summaries for quick status overview including:
  - **Sync Results**: Shows if changes were detected, PR created, or sync skipped
  - **Activity Status**: Reports on recent commits in the mirror repository
  - **Error Details**: Provides specific failure information for troubleshooting
- Examine logs for detailed troubleshooting information

### Step Summary Examples

- **Successful Sync**: Shows PR number and URL
- **No Changes**: Confirms mirror is up to date
- **Skipped Sync**: Reports no recent activity in specified period
- **Failed Sync**: Details authentication or merge conflict issues