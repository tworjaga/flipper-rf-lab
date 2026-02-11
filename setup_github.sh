#!/bin/bash
# Setup script for GitHub repository
# Run this after creating your GitHub repository

REPO_URL="https://github.com/tworjaga/flipper-rf-lab.git"

echo "=========================================="
echo "Flipper RF Lab - GitHub Setup"
echo "=========================================="
echo ""

# Check if git is initialized
if [ ! -d .git ]; then
    echo "Initializing git repository..."
    git init
else
    echo "Git repository already initialized"
fi

# Add all files
echo "Adding files to git..."
git add .

# Initial commit
echo "Creating initial commit..."
git commit -m "Initial commit: Flipper RF Lab v1.0.0

- 15 professional RF analysis features
- Complete documentation
- 30/30 algorithm tests passing
- MIT License
- GitHub Actions CI/CD"

# Add remote
echo "Adding remote origin..."
git remote add origin $REPO_URL 2>/dev/null || echo "Remote already exists"

# Push to GitHub
echo "Pushing to GitHub..."
git branch -M main
git push -u origin main

echo ""
echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "Repository URL: $REPO_URL"
echo ""
echo "Next steps:"
echo "1. Enable GitHub Actions: Settings -> Actions -> Allow all actions"
echo "2. Add repository description and topics"
echo "3. Create a release with .fap file"
echo "4. Submit to Flipper App Catalog"
echo ""
echo "View your repository:"
echo "  $REPO_URL"
